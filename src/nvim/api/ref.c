/// API functions used to deal with Reference objects
///
/// @note API manipulates Reference type, but Reference type objects are
///       constructed by a dispatcher.

#include "nvim/api/private/defs.h"
#include "nvim/api/private/helpers.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "api/ref.c.generated.h"
#endif

/// Like vim_eval(), but returns Reference objects for containers and functions
Object ref_eval(String expr, Error *err)
  FUNC_ATTR_NONNULL_ALL
{
  return NIL;
}

/// Coerce a container Reference to an object
///
/// @param[in]  ref  Reference to coerce.
/// @param[out]  err  Location where error will be saved.
///
/// @return Container (Array or Dictionary) object or original reference.
Object ref_coerce_to_obj(Reference ref, Error *err)
  FUNC_ATTR_NONNULL_ALL
{
  if (ref.def->container.coerce_to_object) {
    return ref.def->container.coerce_to_object(ref, err);
  } else {
    return REFERENCE_OBJ(ref);
  }
}

/// Clear a container referenced by ref
///
/// @param[out]  ref  Container to clear.
/// @param[out]  err  Location where error will be saved.
void ref_clear(Reference ref, Error *err)
  FUNC_ATTR_NONNULL_ALL
{
  if (ref.def->container.clear) {
    ref.def->container.clear(ref, err);
  } else {
    api_set_error(err, Validation, _("Referenced item cannot be cleared"));
  }
}

// TODO(ZyX-I): provide ref_\* API functions for all functions in ReferenceDef.
