/**
 * \file
 * The LWK doesn't have lock dependency tracking, so this is a bunch of NOPs.
 */

#ifndef __LINUX_LOCKDEP_H
#define __LINUX_LOCKDEP_H

#define SINGLE_DEPTH_NESTING                    1


struct lock_class_key { };

#define lockdep_set_class(lock, key)           do { (void)(key); } while (0)


#endif
