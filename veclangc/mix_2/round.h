/* round.h -- rounding macros from SNIPPETS collection (public domain) */

#ifndef ROUND_H
#define ROUND_H

/* Round x to nearest integer */
#ifndef ROUND
#define ROUND(x)  ((int)((x) >= 0.0 ? ((x) + 0.5) : ((x) - 0.5)))
#endif

/* Floor and ceil wrappers if needed */
#ifndef FLOOR
#define FLOOR(x)  ((int)((x) >= 0.0 ? (x) : ((x) - 0.999999999)))
#endif

#ifndef CEIL
#define CEIL(x)   ((int)((x) == (int)(x) ? (x) : (int)(x) + ((x) > 0)))
#endif

#endif /* ROUND_H */
