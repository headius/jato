/*
 * Copyright (c) 2005-2010  Pekka Enberg
 *
 * This file is released under the GPL version 2 with the following
 * clarification and special exception:
 *
 *     Linking this library statically or dynamically with other modules is
 *     making a combined work based on this library. Thus, the terms and
 *     conditions of the GNU General Public License cover the whole
 *     combination.
 *
 *     As a special exception, the copyright holders of this library give you
 *     permission to link this library with independent modules to produce an
 *     executable, regardless of the license terms of these independent
 *     modules, and to copy and distribute the resulting executable under terms
 *     of your choice, provided that you also meet, for each linked independent
 *     module, the terms and conditions of the license of that module. An
 *     independent module is a module which is not derived from or based on
 *     this library. If you modify this library, you may extend this exception
 *     to your version of the library, but you are not obligated to do so. If
 *     you do not wish to do so, delete this exception statement from your
 *     version.
 *
 * Please refer to the file LICENSE for details.
 */

#include "arch/memory.h"

#include "jit/compiler.h"
#include "jit/cu-mapping.h"
#include "jit/emit-code.h"
#include "jit/exception.h"

#include "vm/stack-trace.h"
#include "vm/natives.h"
#include "vm/preload.h"
#include "vm/method.h"
#include "vm/method.h"
#include "vm/class.h"
#include "vm/die.h"
#include "vm/jni.h"
#include "vm/vm.h"
#include "vm/errors.h"

#include "lib/buffer.h"
#include "lib/string.h"

#include <stdio.h>

static void *jit_jni_trampoline(struct compilation_unit *cu)
{
	struct vm_method *method = cu->method;
	struct buffer *buf;
	void *target;

	target = vm_jni_lookup_method(method->class->name, method->name, method->type);
	if (!target) {
		signal_new_exception(vm_java_lang_UnsatisfiedLinkError, "%s.%s%s",
				     method->class->name, method->name, method->type);
		return rethrow_exception();
	}

	if (add_cu_mapping((unsigned long)target, cu))
		return NULL;

	buf = alloc_exec_buffer();
	if (!buf)
		return NULL;

	emit_jni_trampoline(buf, method, target);

	cu->entry_point = buffer_ptr(buf);

	return cu->entry_point;
}

static void *jit_java_trampoline(struct compilation_unit *cu)
{
	int err;

	err = compile(cu);
	if (err) {
		assert(exception_occurred() != NULL);
		return NULL;
	}

	err = add_cu_mapping((unsigned long)cu_entry_point(cu), cu);
	if (err)
		return throw_oom_error();

	return cu_entry_point(cu);
}

void *jit_magic_trampoline(struct compilation_unit *cu)
{
	struct vm_method *method = cu->method;
	void *ret;

	if (opt_trace_magic_trampoline)
		trace_magic_trampoline(cu);

	if (vm_method_is_static(method)) {
		/* This is for "invokestatic"... */
		if (vm_class_ensure_init(method->class))
			return rethrow_exception();
	}

	enum compile_lock_status status;

	status = compile_lock_enter(&cu->compile_lock);

	if (status == STATUS_COMPILED_OK) {
		ret = cu_entry_point(cu);
		goto out_fixup;
	} else if (status == STATUS_COMPILED_ERRONOUS) {
		signal_new_exception(vm_java_lang_Error, "%s.%s%s is erronous",
				     cu->method->class->name,
				     cu->method->name,
				     cu->method->type);
		return rethrow_exception();
	}

	assert(status == STATUS_COMPILING);

	if (vm_method_is_native(cu->method))
		ret = jit_jni_trampoline(cu);
	else
		ret = jit_java_trampoline(cu);

	shrink_compilation_unit(cu);

	status = ret ? STATUS_COMPILED_OK : STATUS_INITIAL;
	compile_lock_leave(&cu->compile_lock, status);

out_fixup:
	if (!ret)
		return rethrow_exception();

	fixup_direct_calls(method->trampoline, (unsigned long) ret);
	return ret;
}

struct jit_trampoline *build_jit_trampoline(struct compilation_unit *cu)
{
	struct jit_trampoline *ret;

	ret = alloc_jit_trampoline();
	if (!ret)
		return NULL;

	emit_trampoline(cu, jit_magic_trampoline, ret);
	add_cu_mapping((unsigned long) buffer_ptr(ret->objcode), cu);

	return ret;
}

void jit_no_such_method_stub(void)
{
	signal_new_exception(vm_java_lang_NoSuchMethodError, NULL);
}
