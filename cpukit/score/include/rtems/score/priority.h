/**
 * @file rtems/score/priority.h
 *
 * @brief Thread Priority Manipulation Routines
 *
 * This include file contains all thread priority manipulation routines.
 * This Handler provides mechanisms which can be used to
 * initialize and manipulate thread priorities.
 */

/*
 *  COPYRIGHT (c) 1989-2011.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.org/license/LICENSE.
 */

#ifndef _RTEMS_SCORE_PRIORITY_H
#define _RTEMS_SCORE_PRIORITY_H

/**
 *  @defgroup ScorePriority Priority Handler
 *
 *  @ingroup Score
 *
 *  This handler encapsulates functionality which is used to manage
 *  thread priorities.  At the SuperCore level 256 priority levels
 *  are supported with lower numbers representing logically more important
 *  threads.  The priority level 0 is reserved for internal RTEMS use.
 *  Typically it is assigned to threads which defer internal RTEMS
 *  actions from an interrupt to thread level to improve interrupt response.
 *  Priority level 255 is assigned to the IDLE thread and really should not
 *  be used by application threads.  The default IDLE thread implementation
 *  is an infinite "branch to self" loop which never yields to other threads
 *  at the same priority.
 */
/**@{*/

/*
 * Processor specific information.
 */
#include <rtems/score/cpu.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  The following type defines the control block used to manage
 *  thread priorities.
 *
 *  @note Priority 0 is reserved for internal threads only.
 */
typedef uint64_t Priority_Control;

/** This defines the highest (most important) thread priority. */
#define PRIORITY_MINIMUM      0

/**
 * @brief This defines the priority of pseudo-ISR threads.
 *
 * Examples are the MPCI and timer server threads.
 */
#define PRIORITY_PSEUDO_ISR   PRIORITY_MINIMUM

/** This defines the default lowest (least important) thread priority. */
#if defined (CPU_PRIORITY_MAXIMUM)
  #define PRIORITY_DEFAULT_MAXIMUM      CPU_PRIORITY_MAXIMUM
#else
  #define PRIORITY_DEFAULT_MAXIMUM      255
#endif

#ifdef __cplusplus
}
#endif

/**@}*/

#endif
/* end of include file */
