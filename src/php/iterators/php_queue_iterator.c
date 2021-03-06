#include "../../common.h"
#include "../objects/php_queue.h"
#include "php_queue_iterator.h"

static void php_ds_queue_iterator_dtor(zend_object_iterator *iter)
{
    zval tmp;
    ZVAL_OBJ(&tmp, (zend_object *) ((ds_queue_iterator_t *) iter)->queue);
    zval_ptr_dtor(&tmp);
}

static int php_ds_queue_iterator_valid(zend_object_iterator *iter)
{
    return Z_ISUNDEF(iter->data) ? FAILURE : SUCCESS;
}

static zval *php_ds_queue_iterator_get_current_data(zend_object_iterator *iter)
{
    return &iter->data;
}

static void php_ds_queue_iterator_get_current_key(zend_object_iterator *iter, zval *key) {
    ZVAL_LONG(key, ((ds_queue_iterator_t *) iter)->position);
}

static void php_ds_queue_iterator_set_current(php_ds_queue_t *queue, zval *data)
{
    if (QUEUE_IS_EMPTY(queue->queue)) {
        ZVAL_UNDEF(data);
    } else {
        ds_queue_pop(queue->queue, data);
        Z_TRY_DELREF_P(data);
    }
}

static void php_ds_queue_iterator_move_forward(zend_object_iterator *iter)
{
    ds_queue_iterator_t *iterator = (ds_queue_iterator_t *) iter;
    php_ds_queue_iterator_set_current(iterator->queue, &iter->data);
    iterator->position++;
}

static void php_ds_queue_iterator_rewind(zend_object_iterator *iter)
{
    ds_queue_iterator_t *iterator = (ds_queue_iterator_t *) iter;
    php_ds_queue_iterator_set_current(iterator->queue, &iter->data);
    iterator->position = 0;
}

static zend_object_iterator_funcs php_ds_queue_iterator_funcs = {
    php_ds_queue_iterator_dtor,
    php_ds_queue_iterator_valid,
    php_ds_queue_iterator_get_current_data,
    php_ds_queue_iterator_get_current_key,
    php_ds_queue_iterator_move_forward,
    php_ds_queue_iterator_rewind
};

zend_object_iterator *php_ds_queue_get_iterator(zend_class_entry *ce, zval *object, int by_ref)
{
    ds_queue_iterator_t *iterator;

    if (by_ref) {
        ITERATION_BY_REF_NOT_SUPPORTED();
        return NULL;
    }

    iterator = ecalloc(1, sizeof(ds_queue_iterator_t));
    zend_iterator_init((zend_object_iterator*) iterator);

    iterator->intern.funcs = &php_ds_queue_iterator_funcs;
    iterator->queue        = (php_ds_queue_t*) (Z_OBJ_P(object));
    iterator->position     = 0;

    // Add a reference to the object so that it doesn't get collected when
    // the iterated object is implict, eg. foreach ($obj->getInstance() as $value){ ... }
#if PHP_VERSION_ID >= 70300
    GC_ADDREF((zend_object *) iterator->queue);
#else
    ++GC_REFCOUNT((zend_object *) iterator->queue);
#endif

    return (zend_object_iterator *) iterator;
}
