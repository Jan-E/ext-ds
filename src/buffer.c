#include <php.h>
#include "buffer.h"

zend_class_entry *ds_buffer_ce;

static zend_object_handlers ds_buffer_handlers;

/******************************************************************************/
/*                             BUFFER ITERATOR                                */
/******************************************************************************/

static void ds_buffer_iterator_dtor(zend_object_iterator *iterator)
{
    zval_ptr_dtor(&iterator->data);
}

static int ds_buffer_iterator_valid(zend_object_iterator *iterator)
{
    ds_buffer_iterator_t *iter = (ds_buffer_iterator_t *) iterator;

    return iter->pos < iter->len ? SUCCESS : FAILURE;
}

static zval *ds_buffer_iterator_get_current_data(zend_object_iterator *iterator)
{
    ds_buffer_iterator_t *iter = (ds_buffer_iterator_t *) iterator;

    return ds_buffer_get(DS_ZVAL_GET_BUFFER(&iterator->data), iter->pos + iter->offset);
}

static void ds_buffer_iterator_get_current_key(zend_object_iterator *iterator, zval *key)
{
    ds_buffer_iterator_t *iter = (ds_buffer_iterator_t *) iterator;

    ZVAL_LONG(key, iter->pos);
}

static void ds_buffer_iterator_move_forward(zend_object_iterator *iterator)
{
    ds_buffer_iterator_t *iter = (ds_buffer_iterator_t *) iterator;

    iter->pos++;
}

static void ds_buffer_iterator_rewind(zend_object_iterator *iterator)
{
    ds_buffer_iterator_t *iter = (ds_buffer_iterator_t *) iterator;

    iter->pos = 0;
}

static zend_object_iterator_funcs ds_buffer_iterator_functions = {
    ds_buffer_iterator_dtor,
    ds_buffer_iterator_valid,
    ds_buffer_iterator_get_current_data,
    ds_buffer_iterator_get_current_key,
    ds_buffer_iterator_move_forward,
    ds_buffer_iterator_rewind
};

/**
 * Creates a new buffer iterator starting at a given offset for a given length.
 */
zend_object_iterator *ds_buffer_iterator(ds_buffer_t *buffer, zend_long offset, zend_long len)
{
    ds_buffer_iterator_t *iter = ecalloc(1, sizeof(ds_buffer_iterator_t));

    zend_iterator_init(&iter->intern);

    iter->intern.funcs = &ds_buffer_iterator_functions;
    iter->offset       = offset;
    iter->len          = len;
    iter->pos          = 0;

    /* Set the buffer as the iterator's internal data. */
    ZVAL_OBJ(&iter->intern.data, buffer);
    Z_TRY_ADDREF(iter->intern.data);

    return (zend_object_iterator *) iter;
}


/******************************************************************************/
/*                                  BUFFER                                    */
/******************************************************************************/

ds_buffer_t *ds_buffer(zend_long capacity)
{
    ds_buffer_t *buffer = ecalloc(1, DS_BUFFER_ALLOC_SIZE(capacity));

    buffer->handlers = &ds_buffer_handlers;

    zend_object_std_init(buffer, ds_buffer_ce);

    DS_BUFFER_SIZE(buffer) = capacity;

    return buffer;
}

/**
 * Destructor for each value in the buffer (between 0 and DS_BUFFER_USED).
 */
static void ds_buffer_dtor_values(ds_buffer_t *buffer)
{
    zval *value;

    DS_BUFFER_FOREACH(buffer, value, {
        i_zval_ptr_dtor(value);
    });
}

/**
 * Buffer destructor.
 */
static void ds_buffer_free_object(zend_object *obj)
{
    ds_buffer_dtor_values(obj);
}

/**
 * Writes the contents of the buffer to an array.
 */
void ds_buffer_to_array(zval *arr, ds_buffer_t *buffer, zend_long len)
{
    zval *value;

    array_init_size(arr, DS_BUFFER_USED(buffer));

    DS_BUFFER_FOREACH(buffer, value, {
        Z_TRY_ADDREF_P(value);
        add_next_index_zval(arr, value);
    });
}

/**
 * Returns the value at given offset in the buffer.
 */
zval *ds_buffer_get(ds_buffer_t *buffer, zend_long offset)
{
    return DS_BUFFER_DATA(buffer) + offset;
}

/**
 * Sets the value of given offset in the buffer.
 */
void ds_buffer_set(ds_buffer_t *buffer, zend_long offset, zval *value)
{
    ZVAL_COPY(ds_buffer_get(buffer, offset), value);
}

/**
 * Reallocates the buffer to a new capacity.
 */
ds_buffer_t *ds_buffer_realloc(ds_buffer_t *buffer, zend_long capacity)
{
    /* A buffer should not be shared when adjusting its capacity. */
    ZEND_ASSERT(GC_REFCOUNT(buffer) == 1);

    /* This operation should never be a no-op. */
    ZEND_ASSERT(capacity != DS_BUFFER_SIZE(buffer));

    /* Verify that we are not truncating values in use. */
    ZEND_ASSERT(capacity > DS_BUFFER_USED(buffer));

    /* Verify that we are not truncating to zero, or beyond. */
    ZEND_ASSERT(capacity > 0);

    /* Make sure that we do not leave a dangling pointer in the gc root buffer. */
    GC_REMOVE_FROM_BUFFER(buffer);

    /* Re-allocate the entire object, including the existing buffer */
    buffer = erealloc(buffer, DS_BUFFER_ALLOC_SIZE(capacity));

    /* Update the size of the buffer. */
    DS_BUFFER_SIZE(buffer) = capacity;

    return buffer;
}

/**
 * Creates a copy of the given buffer.
 */
ds_buffer_t *ds_buffer_create_copy(ds_buffer_t *buffer)
{
    zend_long i;

    /* Create a new buffer using the capacity of the given buffer. */
    ds_buffer_t *copy = ds_buffer(DS_BUFFER_SIZE(buffer));

    zval *src = DS_BUFFER_DATA(buffer);
    zval *dst = DS_BUFFER_DATA(copy);

    for (i = 0; i < DS_BUFFER_USED(buffer); i++) {
        ZVAL_COPY(dst + i, src + i);
    }

    /* Set number of used slots on the new buffer. */
    DS_BUFFER_USED(copy) = DS_BUFFER_USED(buffer);

    return copy;
}

/**
 * GC handler.
 */
static HashTable *ds_buffer_get_gc(zval *obj, zval **gc_data, int *gc_count)
{
    ds_buffer_t *buffer = DS_ZVAL_GET_BUFFER(obj);

    *gc_data  = DS_BUFFER_DATA(buffer);
    *gc_count = DS_BUFFER_SIZE(buffer);

    return NULL;
}

/**
 * Register buffer class entry.
 */
void ds_register_buffer()
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "__ds_buffer", NULL);
    ds_buffer_ce = zend_register_internal_class(&ce);

    /* */
    ds_buffer_ce->ce_flags |= (ZEND_ACC_FINAL | ZEND_ACC_ABSTRACT);

    memcpy(&ds_buffer_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

    ds_buffer_handlers.get_gc   = ds_buffer_get_gc;
    ds_buffer_handlers.free_obj = ds_buffer_free_object;
}
