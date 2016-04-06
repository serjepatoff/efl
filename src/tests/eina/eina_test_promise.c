/* EINA - EFL data type library
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library;
 * if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_EVIL
# include <Evil.h>
#endif

#include <Eina.h>

#include "eina_suite.h"

static void
_eina_test_promise_cb(void* data, void* value EINA_UNUSED)
{
   *(Eina_Bool*)data = EINA_TRUE;
}

START_TEST(eina_test_promise_normal_lifetime)
{
   Eina_Promise_Owner* promise_owner;
   Eina_Promise* promise;
   Eina_Bool ran = EINA_FALSE;

   eina_init();

   promise_owner = eina_promise_default_add(0);

   promise = eina_promise_owner_promise_get(promise_owner);

   eina_promise_then(promise, &_eina_test_promise_cb, NULL, &ran);
   eina_promise_owner_value_set(promise_owner, NULL, NULL);

   ck_assert(ran == EINA_TRUE);

   eina_shutdown();
}
END_TEST

START_TEST(eina_test_promise_normal_lifetime_all)
{
   Eina_Promise_Owner* promise_owner;
   Eina_Promise* first[2] = {NULL, NULL};
   Eina_Promise* promise;
   Eina_Bool ran = EINA_FALSE;

   eina_init();

   promise_owner = eina_promise_default_add(0);
   first[0] = eina_promise_owner_promise_get(promise_owner);
   promise = eina_promise_all(eina_carray_iterator_new((void**)&first[0]));

   eina_promise_then(promise, &_eina_test_promise_cb, NULL, &ran);
   eina_promise_owner_value_set(promise_owner, NULL, NULL);

   ck_assert(ran == EINA_TRUE);

   eina_shutdown();
}
END_TEST

START_TEST(eina_test_promise_immediate_set_lifetime)
{
   Eina_Promise_Owner* owner;
   Eina_Promise* promise;
   Eina_Bool ran = EINA_FALSE;

   eina_init();

   owner = eina_promise_default_add(0);
   promise = eina_promise_owner_promise_get(owner);

   eina_promise_owner_value_set(owner, NULL, NULL);
   eina_promise_then(promise, &_eina_test_promise_cb, NULL, &ran);

   ck_assert(ran == EINA_TRUE);

   eina_shutdown();
}
END_TEST

START_TEST(eina_test_promise_immediate_set_lifetime_all)
{
   Eina_Promise_Owner* owner;
   Eina_Promise* first[2] = {NULL, NULL};
   Eina_Promise* promise;
   Eina_Bool ran = EINA_FALSE;

   eina_init();

   owner = eina_promise_default_add(0);
   first[0] = eina_promise_owner_promise_get(owner);
   promise = eina_promise_all(eina_carray_iterator_new((void**)&first[0]));

   eina_promise_owner_value_set(owner, NULL, NULL);
   eina_promise_then(promise, &_eina_test_promise_cb, NULL, &ran);

   ck_assert(ran == EINA_TRUE);

   eina_shutdown();
}
END_TEST

static void cancel_callback(void* data, Eina_Promise_Owner* promise EINA_UNUSED)
{
   *(Eina_Bool*)data = EINA_TRUE;
}

static void _cancel_promise_callback(void* data EINA_UNUSED, Eina_Error const* value)
{
   ck_assert(!!value);
   *(Eina_Bool*)data = EINA_TRUE;
}

START_TEST(eina_test_promise_cancel_promise)
{
   Eina_Bool ran = EINA_FALSE, cancel_ran = EINA_FALSE;
   Eina_Promise_Owner* owner;
   Eina_Promise* promise;

   eina_init();

   owner = eina_promise_default_add(0);
   eina_promise_owner_default_cancel_cb_add(owner, &cancel_callback, &cancel_ran, NULL);

   promise = eina_promise_owner_promise_get(owner);

   eina_promise_then(promise, NULL, &_cancel_promise_callback, &ran);

   eina_promise_cancel(promise);

   ck_assert(cancel_ran && ran);

   eina_shutdown();
}
END_TEST

void progress_callback(void* data, void* value)
{
   int* i = value;
   ck_assert(*i == 1);
   *(Eina_Bool*)data = EINA_TRUE;
}

START_TEST(eina_test_promise_progress)
{
   Eina_Bool progress_ran = EINA_FALSE;
   Eina_Promise_Owner* owner;
   Eina_Promise* promise;
   int i = 1;

   eina_init();

   owner = eina_promise_default_add(0);

   promise = eina_promise_owner_promise_get(owner);
   eina_promise_progress_cb_add(promise, &progress_callback, &progress_ran);

   eina_promise_owner_progress(owner, &i);

   ck_assert(progress_ran);

   eina_shutdown();
}
END_TEST

void
eina_test_promise(TCase *tc)
{
   tcase_add_test(tc, eina_test_promise_normal_lifetime);
   tcase_add_test(tc, eina_test_promise_normal_lifetime_all);
   tcase_add_test(tc, eina_test_promise_immediate_set_lifetime);
   tcase_add_test(tc, eina_test_promise_immediate_set_lifetime_all);
   tcase_add_test(tc, eina_test_promise_cancel_promise);
   tcase_add_test(tc, eina_test_promise_progress);
}