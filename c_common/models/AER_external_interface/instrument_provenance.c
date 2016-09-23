#include <common-typedefs.h>
#include "PRAERIE-proc-typedefs.h"

/* wildly ugly situation here: simulation.c does not provide any facility for
   extending provenance information directly. Thus we need to store it as an
   internally-defined region in each application. On the other hand in this
   PRAERIE interface implementation, we would like to separate out
   instrumentation from other functionality so that additional provenance
   information may or may not be recorded depending on the particular binary
   chosen. It is desirable to include all the additional functionality in a
   single header rather than declaring a provenance data region as a separate
   extern for each module (which is the other way this can be done). The trouble
   is, we would like to be able to access provenance data for each separate
   module with an inline function: call overhead and jumping is clearly overkill
   for simply updating fields. Inlining (obviously) requires the data to be
   accessible and it is an aggregate, so this will require an external
   declaration in instrumentation.h. The data itself can't be defined in 
   instrumentation.c (the logical place for it) because then it will conflict 
   with the external declaration in the header. Thus rather ridiculously it 
   needs to go in a separate file like this.  
 */

uint prov_data[NUM_PROV_REGIONS]; // structure containing all the provenance data.
