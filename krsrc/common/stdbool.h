#ifndef KRC_STDBOOL_H
#define KRC_STDBOOL_H

/* Minimal stdbool.h for krc runtime compilation. */

#ifndef __cplusplus
#define bool _Bool
#define true 1
#define false 0
#define __bool_true_false_are_defined 1
#endif

#endif /* KRC_STDBOOL_H */
