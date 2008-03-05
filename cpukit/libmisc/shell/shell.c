/*
 *
 *  Instantatiate a new terminal shell.
 *
 *  Author:
 *
 *   WORK: fernando.ruiz@ctv.es
 *   HOME: correo@fernando-ruiz.com
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.com/license/LICENSE.
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <time.h>

#include <rtems.h>
#include <rtems/error.h>
#include <rtems/libio.h>
#include <rtems/libio_.h>
#include <rtems/system.h>
#include <rtems/shell.h>
#include <rtems/shellconfig.h>
#include "internal.h"

#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>

rtems_shell_env_t  rtems_global_shell_env;
rtems_shell_env_t *rtems_current_shell_env;

/*
 *  Initialize the shell user/process environment information
 */
rtems_shell_env_t *rtems_shell_init_env(
  rtems_shell_env_t *shell_env_arg
)
{
  rtems_shell_env_t *shell_env;
  
  if (rtems_global_shell_env.magic != 0x600D600d) {
    rtems_global_shell_env.magic         = 0x600D600d;
    rtems_global_shell_env.devname       = "";
    rtems_global_shell_env.taskname      = "GLOBAL";
    rtems_global_shell_env.tcflag        = 0;
    rtems_global_shell_env.exit_shell    = 0;
    rtems_global_shell_env.forever       = TRUE;
    rtems_global_shell_env.input         = 0;
    rtems_global_shell_env.output        = 0;
    rtems_global_shell_env.output_append = 0;
  }

  shell_env = shell_env_arg;

  if ( !shell_env ) {
    shell_env = malloc(sizeof(rtems_shell_env_t));
    if ( !shell_env )
      return NULL;
    *shell_env = rtems_global_shell_env;
    shell_env->taskname = NULL;
  }

  return shell_env;
}

/*
 *  Get a line of user input with modest features
 */
int rtems_shell_scanline(char * line,int size,FILE * in,FILE * out) {
  int c,col;

  col = 0;
  if (*line) {
    col = strlen(line);
    if (out) fprintf(out,"%s",line);
  }
  tcdrain(fileno(in));
  if (out)
    tcdrain(fileno(out));
  for (;;) {
    line[col] = 0;
    c = fgetc(in);
    switch (c) {
      case 0x04:/*Control-d*/
        if (col)
          break;
      case EOF:
        return 0;
      case '\f':
        if (out)
          fputc('\f',out);
      case 0x03:/*Control-C*/
        line[0] = 0;
      case '\n':
      case '\r':
        if (out)
          fputc('\n',out);
        return 1;
      case  127:
      case '\b':
        if (col) {
          if (out) {
            fputc('\b',out);
            fputc(' ',out);
            fputc('\b',out);
          }
          col--;
        } else {
          if (out) fputc('\a',out);
        }
        break;
     default:
       if (!iscntrl(c)) {
         if (col<size-1) {
            line[col++] = c;
            if (out) fputc(c,out);
          } else {
            if (out) fputc('\a',out);
          }
       } else {
        if (out)
          if (c=='\a') fputc('\a',out);
       }
       break;
     }
  }
}

/* ----------------------------------------------- *
 * - The shell TASK
 * Poor but enough..
 * TODO: Redirection. Tty Signals. ENVVARs. Shell language.
 * ----------------------------------------------- */

void rtems_shell_init_issue(void) {
  static char issue_inited=FALSE;
  struct stat buf;

  if (issue_inited)
    return;
  issue_inited = TRUE;

  /* dummy call to init /etc dir */
  getpwnam("root");

  if (stat("/etc/issue",&buf)) {
    rtems_shell_write_file("/etc/issue",
                           "Welcome to @V\\n"
                           "Login into @S\\n");
  }

  if (stat("/etc/issue.net",&buf)) {
     rtems_shell_write_file("/etc/issue.net",
                            "Welcome to %v\n"
                            "running on %m\n");
  }
}

int rtems_shell_login(FILE * in,FILE * out) {
  FILE          *fd;
  int            c;
  time_t         t;
  int            times;
  char           name[128];
  char           pass[128];
  struct passwd *passwd;

  rtems_shell_init_issue();
  setuid(0);
  setgid(0);
  rtems_current_user_env->euid =
  rtems_current_user_env->egid =0;

  if (out) {
    if ((rtems_current_shell_env->devname[5]!='p')||
        (rtems_current_shell_env->devname[6]!='t')||
        (rtems_current_shell_env->devname[7]!='y')) {
      fd = fopen("/etc/issue","r");
      if (fd) {
        while ((c=fgetc(fd))!=EOF) {
          if (c=='@')  {
            switch(c=fgetc(fd)) {
              case 'L':
                fprintf(out,"%s",rtems_current_shell_env->devname);
                break;
              case 'B':
                fprintf(out,"0");
                break;
              case 'T':
              case 'D':
                time(&t);
                fprintf(out,"%s",ctime(&t));
                break;
              case 'S':
                fprintf(out,"RTEMS");
                break;
              case 'V':
                fprintf(out,"%s\n%s",_RTEMS_version, _Copyright_Notice);
                break;
              case '@':
                fprintf(out,"@");
                break;
              default :
                fprintf(out,"@%c",c);
                break;
            }
          } else if (c=='\\')  {
            switch(c=fgetc(fd)) {
              case '\\': fprintf(out,"\\"); break;
              case 'b':  fprintf(out,"\b"); break;
              case 'f':  fprintf(out,"\f"); break;
              case 'n':  fprintf(out,"\n"); break;
              case 'r':  fprintf(out,"\r"); break;
              case 's':  fprintf(out," ");  break;
              case 't':  fprintf(out,"\t"); break;
              case '@':  fprintf(out,"@");  break;
            }
          } else {
            fputc(c,out);
          }
        }
        fclose(fd);
      }
    } else {
      fd = fopen("/etc/issue.net","r");
      if (fd) {
        while ((c=fgetc(fd))!=EOF) {
          if (c=='%')  {
            switch(c=fgetc(fd)) {
              case 't':
                fprintf(out,"%s",rtems_current_shell_env->devname);
                break;
              case 'h':
                fprintf(out,"0");
                break;
              case 'D':
                fprintf(out," ");
                break;
              case 'd':
                time(&t);
                fprintf(out,"%s",ctime(&t));
                break;
              case 's':
                fprintf(out,"RTEMS");
                break;
              case 'm':
                fprintf(out,"(" CPU_NAME "/" CPU_MODEL_NAME ")");
                break;
              case 'r':
                fprintf(out,_RTEMS_version);
                break;
              case 'v':
                fprintf(out,"%s\n%s",_RTEMS_version,_Copyright_Notice);
	        break;
	      case '%':fprintf(out,"%%");
	        break;
	      default:
                fprintf(out,"%%%c",c);
                break;
            }
          } else {
            fputc(c,out);
          }
        }
        fclose(fd);
      }
    }
  }

  times=0;
  strcpy(name,"");
  strcpy(pass,"");
  for (;;) {
    times++;
    if (times>3) break;
    if (out) fprintf(out,"\nlogin: ");
    if (!rtems_shell_scanline(name,sizeof(name),in,out )) break;
    if (out) fprintf(out,"Password: ");
    if (!rtems_shell_scanline(pass,sizeof(pass),in,NULL)) break;
    if (out) fprintf(out,"\n");
    if ((passwd=getpwnam(name))) {
      if (strcmp(passwd->pw_passwd,"!")) { /* valid user */
        setuid(passwd->pw_uid);
        setgid(passwd->pw_gid);
        rtems_current_user_env->euid =
        rtems_current_user_env->egid = 0;
        chown(rtems_current_shell_env->devname,passwd->pw_uid,0);
        rtems_current_user_env->euid = passwd->pw_uid;
        rtems_current_user_env->egid = passwd->pw_gid;
        if (!strcmp(passwd->pw_passwd,"*")) {
          /* /etc/shadow */
          return 0;
        } else {
          /* crypt() */
          return 0;
        }
      }
    }
    if (out)
      fprintf(out,"Login incorrect\n");
    strcpy(name,"");
    strcpy(pass,"");
  }
  return -1;
}

#if defined(SHELL_DEBUG)
void rtems_shell_print_env(
  rtems_shell_env_t * shell_env
)
{
  if ( !shell_env ) {
    printk( "shell_env is NULL\n" );
    return;
  }
  printk( "shell_env=%p\n"
    "shell_env->magic=0x%08x\t"
    "shell_env->devname=%s\n"
    "shell_env->taskname=%s\t"
    "shell_env->tcflag=%d\n"
    "shell_env->exit_shell=%d\t"
    "shell_env->forever=%d\n",
    shell_env->magic,
    shell_env->devname,
    ((shell_env->taskname) ? shell_env->taskname : "NOT SET"),
    shell_env->tcflag,
    shell_env->exit_shell,
    shell_env->forever
  );
}
#endif

rtems_task rtems_shell_task(rtems_task_argument task_argument)
{
  rtems_shell_env_t *shell_env = (rtems_shell_env_t*) task_argument;
  rtems_id           wake_on_end = shell_env->wake_on_end;
  rtems_shell_main_loop( shell_env );
  if (wake_on_end != RTEMS_INVALID_ID)
    rtems_event_send (wake_on_end, RTEMS_EVENT_1);
  rtems_task_delete( RTEMS_SELF );
}

#define RTEMS_SHELL_MAXIMUM_ARGUMENTS 128

rtems_boolean rtems_shell_main_loop(
  rtems_shell_env_t *shell_env_arg
)
{
  rtems_shell_env_t *shell_env;
  rtems_shell_cmd_t *shell_cmd;
  rtems_status_code  sc;
  struct termios     term;
  char               curdir[256];
  char               cmd[256];
  char               last_cmd[256]; /* to repeat 'r' */
  int                argc;
  char              *argv[RTEMS_SHELL_MAXIMUM_ARGUMENTS];
  rtems_boolean      result = TRUE;
  rtems_boolean      input_file = FALSE;
  int                line = 0;
  FILE              *stdinToClose = NULL;
  FILE              *stdoutToClose = NULL;

  rtems_shell_initialize_command_set();

  shell_env               = 
  rtems_current_shell_env = rtems_shell_init_env( shell_env_arg );
 
  /*
   * @todo chrisj
   * Remove the use of task variables. Change to have a single
   * allocation per shell and then set into a notepad register
   * in the TCB. Provide a function to return the pointer.
   * Task variables are a virus to embedded systems software.
   */
  sc = rtems_task_variable_add(RTEMS_SELF,(void*)&rtems_current_shell_env,free);
  if (sc != RTEMS_SUCCESSFUL) {
    rtems_error(sc,"rtems_task_variable_add(current_shell_env):");
    return FALSE;
  }

  setuid(0);
  setgid(0);
  rtems_current_user_env->euid =
  rtems_current_user_env->egid = 0;

  fileno(stdout);

  /* fprintf( stderr, 
     "-%s-%s-\n", shell_env->input, shell_env->output );
   */

  if (shell_env->output && strcmp(shell_env->output, "stdout") != 0) {
    if (strcmp(shell_env->output, "stderr") == 0) {
      stdout = stderr;
    } else if (strcmp(shell_env->output, "/dev/null") == 0) {
      fclose (stdout);
    } else {
      FILE *output = fopen(shell_env_arg->output,
                           shell_env_arg->output_append ? "a" : "w");
      if (!output) {
        fprintf(stderr, "shell: open output %s failed: %s\n",
                shell_env_arg->output, strerror(errno));
        return FALSE;
      }
      stdout = output;
      stdoutToClose = output;
    }
  }
  
  if (shell_env->input && strcmp(shell_env_arg->input, "stdin") != 0) {
    FILE *input = fopen(shell_env_arg->input, "r");
    if (!input) {
      fprintf(stderr, "shell: open input %s failed: %s\n",
              shell_env_arg->input, strerror(errno));
      return FALSE;
    }
    stdin = input;
    stdinToClose = input;
    shell_env->forever = FALSE;
    input_file = TRUE;
  }
  else {
    /* make a raw terminal,Linux Manuals */
    if (tcgetattr(fileno(stdin), &term) >= 0) {
      term.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
      term.c_oflag &= ~OPOST;
      term.c_oflag |= (OPOST|ONLCR); /* But with cr+nl on output */
      term.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
      if (shell_env->tcflag)
        term.c_cflag = shell_env->tcflag;
      term.c_cflag  |= CLOCAL | CREAD;
      term.c_cc[VMIN]  = 1;
      term.c_cc[VTIME] = 0;
      if (tcsetattr (fileno(stdin), TCSADRAIN, &term) < 0) {
        fprintf(stderr,
                "shell:cannot set terminal attributes(%s)\n",shell_env->devname);
      }
    }
  }

  setvbuf(stdin,NULL,_IONBF,0); /* Not buffered*/
  setvbuf(stdout,NULL,_IONBF,0); /* Not buffered*/

  rtems_shell_initialize_command_set();
  do {
    /* Set again root user and root filesystem, side effect of set_priv..*/
    sc = rtems_libio_set_private_env();
    if (sc != RTEMS_SUCCESSFUL) {
      rtems_error(sc,"rtems_libio_set_private_env():");
      result = FALSE;
      break;
    }
    if (input_file || !rtems_shell_login(stdin,stdout))  {
      const char *c;
      strcpy(last_cmd,"");
      strcpy(cmd,"");
      if (!input_file) {
        rtems_shell_cat_file(stdout,"/etc/motd");
        fprintf(stdout, "\n"
                "RTEMS SHELL (Ver.1.0-FRC):%s. " \
                __DATE__". 'help' to list commands.\n",
                shell_env->devname);
      }
      chdir("/"); /* XXX: chdir to getpwent homedir */
      shell_env->exit_shell = FALSE;
      for (;;) {
        /* Prompt section */
        if (!input_file) {
          /* XXX: show_prompt user adjustable */
          getcwd(curdir,sizeof(curdir));
          fprintf(stdout, "%s%s[%s] %c ",
                  ((shell_env->taskname) ? shell_env->taskname : ""),
                  ((shell_env->taskname) ? " " : ""),
                  curdir,
                  geteuid()?'$':'#');
        }
        
        /* getcmd section */
        if (!rtems_shell_scanline(cmd,sizeof(cmd),stdin,stdout)) {
          break; /*EOF*/
        }
        line++;

        /* evaluate cmd section */
        c = cmd;
        while (*c) {
          if (!isblank(*c))
            break;
          c++;
        }

        if (*c == '\0')                 /* empty line */
          continue;

        if (*c == '#') {                /* comment character */
          cmd[0] = 0;
          continue;
        }

        if (!strcmp(cmd,"e")) {         /* edit last command */
          strcpy(cmd,last_cmd);
          continue;
        } else if (!strcmp(cmd,"r")) {  /* repeat last command */
          strcpy(cmd,last_cmd);
        } else if (!strcmp(cmd,"bye") || !strcmp(cmd,"exit")) {
          fprintf(stdout, "Shell exiting\n" );
          break;
        } else if (!strcmp(cmd,"shutdown")) { /* exit application */
          fprintf(stdout, "System shutting down at user request\n" );
          exit(0);
        } else if (!strcmp(cmd,"")) {    /* only for get a new prompt */
          strcpy(last_cmd,cmd);
        }

        /* exec cmd section */
        /* TODO:
         *  To avoid user crash catch the signals.
         *  Open a new stdio files with posibility of redirection *
         *  Run in a new shell task background. (unix &)
         *  Resuming. A little bash.
         */
        if (!rtems_shell_make_args(cmd, &argc, argv,
                                   RTEMS_SHELL_MAXIMUM_ARGUMENTS)) {
          shell_cmd = rtems_shell_lookup_cmd(argv[0]);
          if ( argv[0] == NULL ) {
            shell_env->errorlevel = -1;
          } else if ( shell_cmd == NULL ) {
            shell_env->errorlevel = rtems_shell_script_file( argc, argv );
          } else {
            shell_env->errorlevel = shell_cmd->command(argc, argv);
          }
        }
        /* end exec cmd section */

        if (shell_env->exit_shell)
          break;
        strcpy(last_cmd, cmd);
        cmd[0] = 0;
      }
      fflush( stdout );
      fflush( stderr );
    }
  } while (result && shell_env->forever);
  if ( stdinToClose )
    fclose( stdinToClose );
  if ( stdoutToClose )
    fclose( stdoutToClose );
  return result;
}

/* ----------------------------------------------- */
static rtems_status_code   rtems_shell_run (
  char                *task_name,
  uint32_t             task_stacksize,
  rtems_task_priority  task_priority,
  char                *devname,
  tcflag_t             tcflag,
  int                  forever,
  const char*          input,
  const char*          output,
  int                  output_append,
  rtems_id             wake_on_end
)
{
  rtems_id           task_id;
  rtems_status_code  sc;
  rtems_shell_env_t *shell_env;
  rtems_name         name;

  if ( task_name )
    name = rtems_build_name(
      task_name[0], task_name[1], task_name[2], task_name[3]);
  else
    name = rtems_build_name( 'S', 'E', 'N', 'V' );

  sc = rtems_task_create(
    name,
    task_priority,
    task_stacksize,
    RTEMS_DEFAULT_MODES,
    RTEMS_LOCAL | RTEMS_FLOATING_POINT,
    &task_id
  );
  if (sc != RTEMS_SUCCESSFUL) {
    rtems_error(sc,"creating task %s in shell_init()",task_name);
    return sc;
  }

  shell_env = rtems_shell_init_env( NULL );
  if ( !shell_env )  {
   rtems_error(RTEMS_NO_MEMORY,
               "allocating shell_env %s in shell_init()",task_name);
   return RTEMS_NO_MEMORY;
  }
  shell_env->devname       = devname;
  shell_env->taskname      = task_name;
  shell_env->tcflag        = tcflag;
  shell_env->exit_shell    = FALSE;
  shell_env->forever       = forever;
  shell_env->input         = strdup (input);
  shell_env->output        = strdup (output);
  shell_env->output_append = output_append;
  shell_env->wake_on_end   = wake_on_end;

  return rtems_task_start(task_id, rtems_shell_task,
                          (rtems_task_argument) shell_env);
}

rtems_status_code   rtems_shell_init (
  char                *task_name,
  uint32_t             task_stacksize,
  rtems_task_priority  task_priority,
  char                *devname,
  tcflag_t             tcflag,
  int                  forever
)
{
  return rtems_shell_run (task_name, task_stacksize, task_priority,
                          devname, tcflag, forever,
                          "stdin", "stdout", 0, RTEMS_INVALID_ID);
}

rtems_status_code   rtems_shell_script (
  char                *task_name,
  uint32_t             task_stacksize,
  rtems_task_priority  task_priority,
  const char*          input,
  const char*          output,
  int                  output_append,
  int                  wait
)
{
  rtems_id          current_task = RTEMS_INVALID_ID;
  rtems_status_code sc;

  if (wait) {
    sc = rtems_task_ident (RTEMS_SELF, RTEMS_LOCAL, &current_task);
    if (sc != RTEMS_SUCCESSFUL)
      return sc;
  }
  
  sc = rtems_shell_run (task_name, task_stacksize, task_priority,
                        NULL, 0, 0, input, output, output_append,
                        current_task);
  if (sc != RTEMS_SUCCESSFUL)
    return sc;

  if (wait) {
    rtems_event_set out;
    sc = rtems_event_receive (RTEMS_EVENT_1, RTEMS_WAIT, 0, &out);
  }

  return sc;
}
