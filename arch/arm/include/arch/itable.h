#ifndef JATO__ARM_ITABLE_H
#define JATO__ARM_ITABLE_H

struct vm_method;
struct vm_object;

void __attribute__((regparm(1)))
itable_resolver_stub_error(struct vm_method *method, struct vm_object *obj);

#endif
