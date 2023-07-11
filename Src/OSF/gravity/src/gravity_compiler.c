//  gravity_compiler.c
//  gravity
//
//  Created by Marco Bambini on 29/08/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//
#include <gravity_.h>
#pragma hdrstop

struct gravity_compiler_t {
	gravity_compiler_t() : parser(0), P_Delegate(0), storage(0), vm(0), ast(0), objects(0)
	{
	}
	gravity_parser_t   * parser;
	gravity_delegate_t * P_Delegate;
	GravityArray <const char *> * storage;
	gravity_vm * vm;
	gnode_t    * ast;
	GravityArray <void *> * objects;
};

static void internal_vm_transfer(gravity_vm * vm, gravity_class_t * obj) 
{
	gravity_compiler_t * compiler = static_cast<gravity_compiler_t *>(gravity_vm_getdata(vm));
	compiler->objects->insert(obj);
}

static void internal_free_class(gravity_hash_t * hashtable, GravityValue key, GravityValue value, void * data) 
{
	// sanity checks
	if(value.IsFunction() && key.IsString()) {
		// check for special function
		gravity_function_t * f = VALUE_AS_FUNCTION(value);
		if(f->tag == EXEC_TYPE_SPECIAL) {
			gravity_function_free(NULL, static_cast<gravity_function_t *>(f->U.Sf.special[0]));
			gravity_function_free(NULL, static_cast<gravity_function_t *>(f->U.Sf.special[1]));
		}
		// a super special init constructor is a string that begins with $init AND it is longer than strlen($init)
		gravity_string_t * s = static_cast<gravity_string_t *>(key);
		bool is_super_function = ((s->len > 5) && (string_casencmp(s->cptr(), CLASS_INTERNAL_INIT_NAME, 5) == 0));
		if(!is_super_function) 
			gravity_function_free(NULL, VALUE_AS_FUNCTION(value));
	}
}

static void internal_vm_cleanup(gravity_vm * vm) 
{
	gravity_compiler_t * compiler = static_cast<gravity_compiler_t *>(gravity_vm_getdata(vm));
	size_t count = compiler->objects->getCount();
	for(size_t i = 0; i < count; ++i) {
		gravity_class_t * obj = static_cast<gravity_class_t *>(compiler->objects->pop());
		if(obj->IsClass()) {
			gravity_class_t * c = (gravity_class_t *)obj;
			gravity_hash_iterate(c->htable, internal_free_class, NULL);
		}
		gravity_object_free(vm, obj);
	}
}

// MARK: -

gravity_compiler_t * gravity_compiler_create(gravity_delegate_t * delegate) 
{
	gravity_compiler_t * compiler = new gravity_compiler_t;
	if(compiler) {
		//compiler->ast = NULL;
		compiler->objects = void_array_create();
		compiler->P_Delegate = delegate;
	}
	return compiler;
}

static void gravity_compiler_reset(gravity_compiler_t * compiler, bool free_core) 
{
	// free memory for array of strings storage
	if(compiler->storage) {
		cstring_array_each(compiler->storage, {mem_free((void *)val);});
		gnode_array_free(compiler->storage);
	}
	// first ast then parser, don't change the release order
	gnode_free(compiler->ast);
	if(compiler->parser) 
		gravity_parser_free(compiler->parser);
	// at the end free mini VM and objects array
	gravity_vm_free(compiler->vm);
	if(compiler->objects) {
		compiler->objects->Z();
		mem_free((void *)compiler->objects);
	}
	// feel free to free core if someone requires it
	if(free_core) 
		gravity_core_free();
	// reset internal pointers
	compiler->vm = NULL;
	compiler->ast = NULL;
	compiler->parser = NULL;
	compiler->objects = NULL;
	compiler->storage = NULL;
}

void gravity_compiler_free(gravity_compiler_t * pCompiler) 
{
	if(pCompiler) {
		gravity_compiler_reset(pCompiler, true);
		//mem_free(pCompiler);
		delete pCompiler;
	}
}

gnode_t * gravity_compiler_ast(gravity_compiler_t * compiler) 
{
	return compiler->ast;
}

void gravity_compiler_transfer(gravity_compiler_t * compiler, gravity_vm * vm) 
{
	if(compiler->objects) {
		// transfer each object from compiler mini VM to exec VM
		gravity_gc_setenabled(vm, false);
		uint count = compiler->objects->getCount();
		for(uint i = 0; i < count; ++i) {
			gravity_class_t * obj = static_cast<gravity_class_t *>(compiler->objects->pop());
			gravity_vm_transfer(vm, obj);
			if(!obj->IsClosure()) 
				continue;
			// $moduleinit closure needs to be explicitly initialized
			gravity_closure_t * closure = (gravity_closure_t *)obj;
			if(closure->f->identifier && sstreq(closure->f->identifier, INITMODULE_NAME)) {
				// code is here because it does not make sense to add this overhead (that needs to be executed
				// only once)
				// inside the gravity_vm_transfer callback which is called for each allocated object inside the VM
				gravity_vm_initmodule(vm, closure->f);
			}
		}
		gravity_gc_setenabled(vm, true);
	}
}

// MARK: -

gravity_closure_t * gravity_compiler_run(gravity_compiler_t * compiler, const char * source, size_t len, uint32 fileid, bool is_static, bool add_debug) 
{
	bool b1 = false;
	bool b2 = false;
	gravity_function_t * f = 0;
	if(!source || !len) 
		return NULL;
	// CHECK cleanup first
	gnode_free(compiler->ast);
	SETIFZ(compiler->objects, void_array_create());
	// CODEGEN requires a mini vm in order to be able to handle garbage collector
	compiler->vm = gravity_vm_newmini();
	gravity_vm_setdata(compiler->vm, (void *)compiler);
	gravity_vm_set_callbacks(compiler->vm, internal_vm_transfer, internal_vm_cleanup);
	gravity_core_register(compiler->vm);
	// STEP 0: CREATE PARSER
	compiler->parser = gravity_parser_create(source, len, fileid, is_static);
	if(!compiler->parser) 
		return NULL;
	// STEP 1: SYNTAX CHECK
	compiler->ast = gravity_parser_run(compiler->parser, compiler->P_Delegate);
	if(!compiler->ast) 
		goto abort_compilation;
	gravity_parser_free(compiler->parser);
	compiler->parser = NULL;
	// STEP 2a: SEMANTIC CHECK (NON-LOCAL DECLARATIONS)
	b1 = gravity_semacheck1(compiler->ast, compiler->P_Delegate);
	if(!b1) 
		goto abort_compilation;
	// STEP 2b: SEMANTIC CHECK (LOCAL DECLARATIONS)
	b2 = gravity_semacheck2(compiler->ast, compiler->P_Delegate);
	if(!b2) 
		goto abort_compilation;
	// STEP 3: INTERMEDIATE CODE GENERATION (stack based VM)
	f = gravity_codegen(compiler->ast, compiler->P_Delegate, compiler->vm, add_debug);
	if(!f) 
		goto abort_compilation;
	// STEP 4: CODE GENERATION (register based VM)
	f = gravity_optimizer(f, add_debug);
	if(f) 
		return gravity_closure_new(compiler->vm, f);
abort_compilation:
	gravity_compiler_reset(compiler, false);
	return NULL;
}

GravityJson * gravity_compiler_serialize(gravity_compiler_t * compiler, gravity_closure_t * closure) 
{
	if(!closure) 
		return NULL;
	GravityJson * json = json_new();
	json_begin_object(json, NULL);
	gravity_function_serialize(closure->f, json);
	json_end_object(json);
	return json;
}

bool gravity_compiler_serialize_infile(gravity_compiler_t * compiler, gravity_closure_t * closure, const char * path) 
{
	if(!closure) 
		return false;
	GravityJson * json = gravity_compiler_serialize(compiler, closure);
	if(!json) 
		return false;
	json_write_file(json, path);
	json_free(json);
	return true;
}
