/*
** upb::Message is a representation for protobuf messages.
**
** However it differs from other common representations like
** google::protobuf::Message in one key way: it does not prescribe any
** ownership between messages and submessages, and it relies on the
** client to delete each message/submessage/array/map at the appropriate
** time.
**
** A client can access a upb::Message without knowing anything about
** ownership semantics, but to create or mutate a message a user needs
** to implement the memory management themselves.
**
** Currently all messages, arrays, and maps store a upb_alloc* internally.
** Mutating operations use this when they require dynamically-allocated
** memory.  We could potentially eliminate this size overhead later by
** letting the user flip a bit on the factory that prevents this from
** being stored.  The user would then need to use separate functions where
** the upb_alloc* is passed explicitly.  However for handlers to populate
** such structures, they would need a place to store this upb_alloc* during
** parsing; upb_handlers don't currently have a good way to accommodate this.
**
** TODO: UTF-8 checking?
**/

#ifndef UPB_MSG_H_
#define UPB_MSG_H_

#include "upb/def.h"
#include "upb/handlers.h"
#include "upb/sink.h"

#ifdef __cplusplus

namespace upb {
class Array;
class Map;
class MapIterator;
class MessageFactory;
class MessageLayout;
class Visitor;
class VisitorPlan;
}

#endif

UPB_DECLARE_TYPE(upb::MessageFactory, upb_msgfactory)
UPB_DECLARE_TYPE(upb::MessageLayout, upb_msglayout)
UPB_DECLARE_TYPE(upb::Array, upb_array)
UPB_DECLARE_TYPE(upb::Map, upb_map)
UPB_DECLARE_TYPE(upb::MapIterator, upb_mapiter)
UPB_DECLARE_TYPE(upb::Visitor, upb_visitor)
UPB_DECLARE_TYPE(upb::VisitorPlan, upb_visitorplan)

/* TODO(haberman): C++ accessors */

UPB_BEGIN_EXTERN_C

typedef void upb_msg;


/** upb_msglayout *************************************************************/

/* upb_msglayout represents the memory layout of a given upb_msgdef.  You get
 * instances of this from a upb_msgfactory, and the factory always owns the
 * msglayout. */

/* Gets the factory for this layout */
upb_msgfactory *upb_msglayout_factory(const upb_msglayout *l);

/* Get the msglayout for a submessage.  This requires that this field is a
 * submessage, ie. upb_fielddef_issubmsg(upb_msglayout_msgdef(l)) == true.
 *
 * Since map entry messages don't have layouts, if upb_fielddef_ismap(f) == true
 * then this function will return the layout for the map's value.  It requires
 * that the value type of the map field is a submessage. */
const upb_msglayout *upb_msglayout_sublayout(const upb_msglayout *l,
                                             const upb_fielddef *f);

/* Returns the msgdef for this msglayout. */
const upb_msgdef *upb_msglayout_msgdef(const upb_msglayout *l);


/** upb_visitor ***************************************************************/

/* upb_visitor will visit all the fields of a message and its submessages.  It
 * uses a upb_visitorplan which you can obtain from a upb_msgfactory. */

upb_visitor *upb_visitor_create(upb_env *e, const upb_visitorplan *vp,
                                upb_sink *output);
bool upb_visitor_visitmsg(upb_visitor *v, const upb_msg *msg);


/** upb_msgfactory ************************************************************/

/* A upb_msgfactory contains a cache of upb_msglayout, upb_handlers, and
 * upb_visitorplan objects.  These are the objects necessary to represent,
 * populate, and and visit upb_msg objects.
 *
 * These caches are all populated by upb_msgdef, and lazily created on demand.
 */

/* Creates and destroys a msgfactory, respectively.  The messages for this
 * msgfactory must come from |symtab| (which should outlive the msgfactory). */
upb_msgfactory *upb_msgfactory_new(const upb_symtab *symtab);
void upb_msgfactory_free(upb_msgfactory *f);

const upb_symtab *upb_msgfactory_symtab(const upb_msgfactory *f);

/* The functions to get cached objects, lazily creating them on demand.  These
 * all require:
 *
 * - m is in upb_msgfactory_symtab(f)
 * - upb_msgdef_mapentry(m) == false (since map messages can't have layouts).
 *
 * The returned objects will live for as long as the msgfactory does.
 *
 * TODO(haberman): consider making this thread-safe and take a const
 * upb_msgfactory. */
const upb_msglayout *upb_msgfactory_getlayout(upb_msgfactory *f,
                                              const upb_msgdef *m);
const upb_handlers *upb_msgfactory_getmergehandlers(upb_msgfactory *f,
                                                    const upb_msgdef *m);
const upb_visitorplan *upb_msgfactory_getvisitorplan(upb_msgfactory *f,
                                                     const upb_handlers *h);


/** upb_msgval ****************************************************************/

/* A union representing all possible protobuf values.  Used for generic get/set
 * operations. */

typedef union {
  bool b;
  float flt;
  double dbl;
  int32_t i32;
  int64_t i64;
  uint32_t u32;
  uint64_t u64;
  const upb_map* map;
  const upb_msg* msg;
  const upb_array* arr;
  const void* ptr;
  struct {
    const char *ptr;
    size_t len;
  } str;
} upb_msgval;

#define ACCESSORS(name, membername, ctype) \
  UPB_INLINE ctype upb_msgval_get ## name(upb_msgval v) { \
    return v.membername; \
  } \
  UPB_INLINE void upb_msgval_set ## name(upb_msgval *v, ctype cval) { \
    v->membername = cval; \
  } \
  UPB_INLINE upb_msgval upb_msgval_ ## name(ctype v) { \
    upb_msgval ret; \
    ret.membername = v; \
    return ret; \
  }

ACCESSORS(bool,   b,   bool)
ACCESSORS(float,  flt, float)
ACCESSORS(double, dbl, double)
ACCESSORS(int32,  i32, int32_t)
ACCESSORS(int64,  i64, int64_t)
ACCESSORS(uint32, u32, uint32_t)
ACCESSORS(uint64, u64, uint64_t)
ACCESSORS(map,    map, const upb_map*)
ACCESSORS(msg,    msg, const upb_msg*)
ACCESSORS(ptr,    ptr, const void*)
ACCESSORS(arr,    arr, const upb_array*)

#undef ACCESSORS

UPB_INLINE upb_msgval upb_msgval_str(const char *ptr, size_t len) {
  upb_msgval ret;
  ret.str.ptr = ptr;
  ret.str.len = len;
  return ret;
}

UPB_INLINE const char* upb_msgval_getstr(upb_msgval val) {
  return val.str.ptr;
}

UPB_INLINE size_t upb_msgval_getstrlen(upb_msgval val) {
  return val.str.len;
}


/** upb_msg *******************************************************************/

/* A upb_msg represents a protobuf message.  It always corresponds to a specific
 * upb_msglayout, which describes how it is laid out in memory.
 *
 * The message will have a fixed size, as returned by upb_msg_sizeof(), which
 * will be used to store fixed-length fields.  The upb_msg may also allocate
 * dynamic memory internally to store data such as:
 *
 * - extensions
 * - unknown fields
 */

/* Returns the size of a message given this layout. */
size_t upb_msg_sizeof(const upb_msglayout *l);

/* upb_msg_init() / upb_msg_uninit() allow the user to use a pre-allocated
 * block of memory as a message.  The block's size should be upb_msg_sizeof().
 * upb_msg_uninit() must be called to release internally-allocated memory
 * unless the allocator is an arena that does not require freeing.
 *
 * Please note that upb_msg_uninit() does *not* free any submessages, maps,
 * or arrays referred to by this message's fields.  You must free them manually
 * yourself. */
void upb_msg_init(upb_msg *msg, const upb_msglayout *l, upb_alloc *a);
void upb_msg_uninit(upb_msg *msg, const upb_msglayout *l);

/* Like upb_msg_init() / upb_msg_uninit(), except the message's memory is
 * allocated / freed from the given upb_alloc. */
upb_msg *upb_msg_new(const upb_msglayout *l, upb_alloc *a);
void upb_msg_free(upb_msg *msg, const upb_msglayout *l);

/* Returns the upb_alloc for the given message. */
upb_alloc *upb_msg_alloc(const upb_msg *msg, const upb_msglayout *l);

/* Packs the tree of messages rooted at "msg" into a single hunk of memory,
 * allocated from the given allocator. */
void *upb_msg_pack(const upb_msg *msg, const upb_msglayout *l,
                   void *p, size_t *ofs, size_t size);

/* Read-only message API.  Can be safely called by anyone. */

/* Returns the value associated with this field:
 *   - for scalar fields (including strings), the value directly.
 *   - return upb_msg*, or upb_map* for msg/map.
 *     If the field is unset for these field types, returns NULL.
 *
 * TODO(haberman): should we let users store cached array/map/msg
 * pointers here for fields that are unset?  Could be useful for the
 * strongly-owned submessage model (ie. generated C API that doesn't use
 * arenas).
 */
upb_msgval upb_msg_get(const upb_msg *msg,
                       const upb_fielddef *f,
                       const upb_msglayout *l);

/* May only be called for fields where upb_fielddef_haspresence(f) == true. */
bool upb_msg_has(const upb_msg *msg,
                 const upb_fielddef *f,
                 const upb_msglayout *l);

/* Returns NULL if no field in the oneof is set. */
const upb_fielddef *upb_msg_getoneofcase(const upb_msg *msg,
                                         const upb_oneofdef *o,
                                         const upb_msglayout *l);

/* Returns true if any field in the oneof is set. */
bool upb_msg_hasoneof(const upb_msg *msg,
                      const upb_oneofdef *o,
                      const upb_msglayout *l);


/* Mutable message API.  May only be called by the owner of the message who
 * knows its ownership scheme and how to keep it consistent. */

/* Sets the given field to the given value.  Does not perform any memory
 * management: if you overwrite a pointer to a msg/array/map/string without
 * cleaning it up (or using an arena) it will leak.
 */
bool upb_msg_set(upb_msg *msg,
                 const upb_fielddef *f,
                 upb_msgval val,
                 const upb_msglayout *l);

/* For a primitive field, set it back to its default. For repeated, string, and
 * submessage fields set it back to NULL.  This could involve releasing some
 * internal memory (for example, from an extension dictionary), but it is not
 * recursive in any way and will not recover any memory that may be used by
 * arrays/maps/strings/msgs that this field may have pointed to.
 */
bool upb_msg_clearfield(upb_msg *msg,
                        const upb_fielddef *f,
                        const upb_msglayout *l);

/* Clears all fields in the oneof such that none of them are set. */
bool upb_msg_clearoneof(upb_msg *msg,
                        const upb_oneofdef *o,
                        const upb_msglayout *l);

/* TODO(haberman): copyfrom()/mergefrom()? */


/** upb_array *****************************************************************/

/* A upb_array stores data for a repeated field.  The memory management
 * semantics are the same as upb_msg.  A upb_array allocates dynamic
 * memory internally for the array elements. */

size_t upb_array_sizeof(upb_fieldtype_t type);
void upb_array_init(upb_array *arr, upb_fieldtype_t type, upb_alloc *a);
void upb_array_uninit(upb_array *arr);
upb_array *upb_array_new(upb_fieldtype_t type, upb_alloc *a);
void upb_array_free(upb_array *arr);

/* Read-only interface.  Safe for anyone to call. */

size_t upb_array_size(const upb_array *arr);
upb_fieldtype_t upb_array_type(const upb_array *arr);
upb_msgval upb_array_get(const upb_array *arr, size_t i);

/* Write interface.  May only be called by the message's owner who can enforce
 * its memory management invariants. */

bool upb_array_set(upb_array *arr, size_t i, upb_msgval val);


/** upb_map *******************************************************************/

/* A upb_map stores data for a map field.  The memory management semantics are
 * the same as upb_msg, with one notable exception.  upb_map will internally
 * store a copy of all string keys, but *not* any string values or submessages.
 * So you must ensure that any string or message values outlive the map, and you
 * must delete them manually when they are no longer required. */

size_t upb_map_sizeof(upb_fieldtype_t ktype, upb_fieldtype_t vtype);
bool upb_map_init(upb_map *map, upb_fieldtype_t ktype, upb_fieldtype_t vtype,
                  upb_alloc *a);
void upb_map_uninit(upb_map *map);
upb_map *upb_map_new(upb_fieldtype_t ktype, upb_fieldtype_t vtype, upb_alloc *a);
void upb_map_free(upb_map *map);

/* Read-only interface.  Safe for anyone to call. */

size_t upb_map_size(const upb_map *map);
upb_fieldtype_t upb_map_keytype(const upb_map *map);
upb_fieldtype_t upb_map_valuetype(const upb_map *map);
bool upb_map_get(const upb_map *map, upb_msgval key, upb_msgval *val);

/* Write interface.  May only be called by the message's owner who can enforce
 * its memory management invariants. */

/* Sets or overwrites an entry in the map.  Return value indicates whether
 * the operation succeeded or failed with OOM, and also whether an existing
 * key was replaced or not. */
bool upb_map_set(upb_map *map,
                 upb_msgval key, upb_msgval val,
                 upb_msgval *valremoved);

/* Deletes an entry in the map.  Returns true if the key was present. */
bool upb_map_del(upb_map *map, upb_msgval key);


/** upb_mapiter ***************************************************************/

/* For iterating over a map.  Map iterators are invalidated by mutations to the
 * map, but an invalidated iterator will never return junk or crash the process.
 * An invalidated iterator may return entries that were already returned though,
 * and if you keep invalidating the iterator during iteration, the program may
 * enter an infinite loop. */

size_t upb_mapiter_sizeof();

void upb_mapiter_begin(upb_mapiter *i, const upb_map *t);
upb_mapiter *upb_mapiter_new(const upb_map *t, upb_alloc *a);
void upb_mapiter_free(upb_mapiter *i, upb_alloc *a);
void upb_mapiter_next(upb_mapiter *i);
bool upb_mapiter_done(const upb_mapiter *i);

upb_msgval upb_mapiter_key(const upb_mapiter *i);
upb_msgval upb_mapiter_value(const upb_mapiter *i);
void upb_mapiter_setdone(upb_mapiter *i);
bool upb_mapiter_isequal(const upb_mapiter *i1, const upb_mapiter *i2);


/** Handlers ******************************************************************/

/* These are the handlers used internally by upb_msgfactory_getmergehandlers().
 * They write scalar data to a known offset from the message pointer.
 *
 * These would be trivial for anyone to implement themselves, but it's better
 * to use these because some JITs will recognize and specialize these instead
 * of actually calling the function. */

/* Sets a handler for the given primitive field that will write the data at the
 * given offset.  If hasbit > 0, also sets a hasbit at the given bit offset
 * (addressing each byte low to high). */
bool upb_msg_setscalarhandler(upb_handlers *h,
                              const upb_fielddef *f,
                              size_t offset,
                              int32_t hasbit);

/* If the given handler is a msghandlers_primitive field, returns true and sets
 * *type, *offset and *hasbit.  Otherwise returns false. */
bool upb_msg_getscalarhandlerdata(const upb_handlers *h,
                                  upb_selector_t s,
                                  upb_fieldtype_t *type,
                                  size_t *offset,
                                  int32_t *hasbit);

UPB_END_EXTERN_C

#endif /* UPB_MSG_H_ */
