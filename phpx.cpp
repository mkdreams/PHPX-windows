/*
  +----------------------------------------------------------------------+
  | PHP-X                                                                |
  +----------------------------------------------------------------------+
  | Copyright (c) 2016-2017 The Swoole Group                             |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the GPL license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.gnu.org/licenses/                                         |
  | If you did not receive a copy of the GPL3.0 license and are unable   |
  | to obtain it through the world-wide-web, please send a note to       |
  | license@swoole.com so we can mail you a copy immediately.            |
  +----------------------------------------------------------------------+
  | Author: Tianfeng Han  <mikan.tenny@gmail.com>                        |
  +----------------------------------------------------------------------+
*/
#pragma once
#include "phpx.h"
extern "C"
{
#include <ext/hash/php_hash.h>
}
using namespace std;

namespace php
{

	int array_data_compare(const void* a, const void* b)
	{
		Bucket* f;
		Bucket* s;
		zval result;
		zval* first;
		zval* second;

		f = (Bucket*)a;
		s = (Bucket*)b;

		first = &f->val;
		second = &s->val;

		if (UNEXPECTED(Z_TYPE_P(first) == IS_INDIRECT))
		{
			first = Z_INDIRECT_P(first);
		}
		if (UNEXPECTED(Z_TYPE_P(second) == IS_INDIRECT))
		{
			second = Z_INDIRECT_P(second);
		}
		if (compare_function(&result, first, second) == FAILURE)
		{
			return 0;
		}

		ZEND_ASSERT(Z_TYPE(result) == IS_LONG);
		return Z_LVAL(result);
	}

	Array Array::slice(long offset, long length, bool preserve_keys)
	{
		size_t num_in = count();

		if (offset > num_in)
		{
			return Array();
		}
		else if (offset < 0 && (offset = (num_in + offset)) < 0)
		{
			offset = 0;
		}

		if (length < 0)
		{
			length = num_in - offset + length;
		}
		else if (((zend_ulong)offset + (zend_ulong)length) > (unsigned) num_in)
		{
			length = num_in - offset;
		}

		if (length <= 0)
		{
			return Array();
		}

		zend_string* string_key;
		zend_ulong num_key;
		zval* entry;

		zval return_value;
		array_init_size(&return_value, (uint32_t)length);

		/* Start at the beginning and go until we hit offset */
		int pos = 0;
		if (!preserve_keys && (Z_ARRVAL_P(this->ptr())->u.flags & HASH_FLAG_PACKED))
		{
			zend_hash_real_init(Z_ARRVAL_P(&return_value), 1);
			ZEND_HASH_FILL_PACKED(Z_ARRVAL_P(&return_value))
			{
				ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(this->ptr()), entry)
				{
					pos++;
					if (pos <= offset)
					{
						continue;
					}
					if (pos > offset + length)
					{
						break;
					}
					ZEND_HASH_FILL_ADD(entry);
					zval_add_ref(entry);
				}
				ZEND_HASH_FOREACH_END();
			}
			ZEND_HASH_FILL_END();
		}
		else
		{
			ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(this->ptr()), num_key, string_key, entry)
			{
				pos++;
				if (pos <= offset)
				{
					continue;
				}
				if (pos > offset + length)
				{
					break;
				}

				if (string_key)
				{
					entry = zend_hash_add_new(Z_ARRVAL_P(&return_value), string_key, entry);
				}
				else
				{
					if (preserve_keys)
					{
						entry = zend_hash_index_add_new(Z_ARRVAL_P(&return_value), num_key, entry);
					}
					else
					{
						entry = zend_hash_next_index_insert_new(Z_ARRVAL_P(&return_value), entry);
					}
				}
				zval_add_ref(entry);
			}
			ZEND_HASH_FOREACH_END();
		}
		Array retval(&return_value);
		return retval;
	}


	unordered_map<string, Resource*> resource_map;
	unordered_map<string, Class*> class_map;
	unordered_map<string, Interface*> interface_map;
	map<const char*, map<const char*, method_t, strCmp>, strCmp> method_map;
	map<const char*, function_t, strCmp> function_map;
	map<int, void*> object_array;
	unordered_map<string, Extension*> _name_to_extension;
	unordered_map<int, Extension*> _module_number_to_extension;

	void error(int level, const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		php_verror(NULL, "", level, format, args);
		va_end(args);
	}

	Variant constant(const char* name)
	{
		zend_string* _name = zend_string_init(name, strlen(name), 0);
		zval* val = zend_get_constant_ex(_name, NULL, ZEND_FETCH_CLASS_SILENT);
		zend_string_free(_name);
		if (val == NULL)
		{
			return nullptr;
		}
		Variant retval(val);
		return retval;
	}

	void echo(const char* format, ...)
	{
		va_list args;
		char* buffer;
		size_t size;

		va_start(args, format);
		size = vspprintf(&buffer, 0, format, args);
		PHPWRITE(buffer, size);
		efree(buffer);
		va_end(args);
	}

#if  PHP_VERSION_ID < 70300
	static int validate_constant_array(HashTable * ht) /* {{{ */
	{
		int ret = 1;
		zval* val;

		ht->u.v.nApplyCount++;
		ZEND_HASH_FOREACH_VAL_IND(ht, val)
		{
			ZVAL_DEREF(val);
			if (Z_REFCOUNTED_P(val))
			{
				if (Z_TYPE_P(val) == IS_ARRAY)
				{
					if (Z_REFCOUNTED_P(val))
					{
						if (Z_ARRVAL_P(val)->u.v.nApplyCount > 0)
						{
							zend_error(E_WARNING, "Constants cannot be recursive arrays");
							ret = 0;
							break;
						}
						else if (!validate_constant_array(Z_ARRVAL_P(val)))
						{
							ret = 0;
							break;
						}
					}
				}
				else if (Z_TYPE_P(val) != IS_STRING && Z_TYPE_P(val) != IS_RESOURCE)
				{
					zend_error(E_WARNING, "Constants may only evaluate to scalar values or arrays");
					ret = 0;
					break;
				}
			}
		}
		ZEND_HASH_FOREACH_END();
		ht->u.v.nApplyCount--;
		return ret;
	}
#else
	static int validate_constant_array(HashTable* ht)
	{
		int ret = 1;
		zval* val;

		GC_PROTECT_RECURSION(ht);
		ZEND_HASH_FOREACH_VAL_IND(ht, val)
		{
			ZVAL_DEREF(val);
			if (Z_REFCOUNTED_P(val))
			{
				if (Z_TYPE_P(val) == IS_ARRAY)
				{
					if (Z_REFCOUNTED_P(val))
					{
						if (Z_IS_RECURSIVE_P(val))
						{
							zend_error(E_WARNING, "Constants cannot be recursive arrays");
							ret = 0;
							break;
						}
						else if (!validate_constant_array(Z_ARRVAL_P(val)))
						{
							ret = 0;
							break;
						}
					}
				}
				else if (Z_TYPE_P(val) != IS_STRING && Z_TYPE_P(val) != IS_RESOURCE)
				{
					zend_error(E_WARNING, "Constants may only evaluate to scalar values, arrays or resources");
					ret = 0;
					break;
				}
			}
		}
		ZEND_HASH_FOREACH_END();
		GC_UNPROTECT_RECURSION(ht);
		return ret;
	}
#endif

	static void copy_constant_array(zval* dst, zval* src) /* {{{ */
	{
		zend_string* key;
		zend_ulong idx;
		zval* new_val, * val;

		array_init_size(dst, zend_hash_num_elements(Z_ARRVAL_P(src)));
		ZEND_HASH_FOREACH_KEY_VAL_IND(Z_ARRVAL_P(src), idx, key, val)
		{
			/* constant arrays can't contain references */
			ZVAL_DEREF(val);
			if (key)
			{
				new_val = zend_hash_add_new(Z_ARRVAL_P(dst), key, val);
			}
			else
			{
				new_val = zend_hash_index_add_new(Z_ARRVAL_P(dst), idx, val);
			}
			if (Z_TYPE_P(val) == IS_ARRAY)
			{
				if (Z_REFCOUNTED_P(val))
				{
					copy_constant_array(new_val, val);
				}
			}
			else if (Z_REFCOUNTED_P(val))
			{
				Z_ADDREF_P(val);
			}
		}
		ZEND_HASH_FOREACH_END();
	}

	bool define(const char* name, const Variant& v, bool case_sensitive)
	{
		size_t len = strlen(name);
		zval* val = const_cast<Variant&>(v).ptr(), val_free;
		zend_constant c;

		/* class constant, check if there is name and make sure class is valid & exists */
		if (zend_memnstr(name, "::", sizeof("::") - 1, name + len))
		{
			zend_error(E_WARNING, "Class constants cannot be defined or redefined");
			return false;
		}

		ZVAL_UNDEF(&val_free);

	repeat: switch (Z_TYPE_P(val))
	{
	case IS_LONG:
	case IS_DOUBLE:
	case IS_STRING:
	case IS_FALSE:
	case IS_TRUE:
	case IS_NULL:
	case IS_RESOURCE:
		break;
	case IS_ARRAY:
		if (Z_REFCOUNTED_P(val))
		{
			if (!validate_constant_array(Z_ARRVAL_P(val)))
			{
				return false;
			}
			else
			{
				copy_constant_array(&c.value, val);
				goto register_constant;
			}
		}
		break;
	case IS_OBJECT:
		if (Z_TYPE(val_free) == IS_UNDEF)
		{
			if (Z_OBJ_HT_P(val)->get)
			{
				zval rv;
				val = Z_OBJ_HT_P(val)->get(val, &rv);
				ZVAL_COPY_VALUE(&val_free, val);
				goto repeat;
			}
			else if (Z_OBJ_HT_P(val)->cast_object)
			{
				if (Z_OBJ_HT_P(val)->cast_object(val, &val_free, IS_STRING) == SUCCESS)
				{
					val = &val_free;
					break;
				}
			}
		}
		/* no break */
	default:
		zend_error(E_WARNING, "Constants may only evaluate to scalar values or arrays");
		zval_ptr_dtor(&val_free);
		return false;
	}

			ZVAL_COPY(&c.value, val);
			zval_ptr_dtor(&val_free);
		register_constant:
#if  PHP_VERSION_ID < 70300
			c.flags = case_sensitive ? CONST_CS : 0; /* non persistent */
			c.module_number = PHP_USER_CONSTANT;
#endif
			c.name = zend_string_init(name, len, 0);
			if (zend_register_constant(&c) == SUCCESS)
			{
				return true;
			}
			else
			{
				return false;
			}
	}

	String number_format(double num, int decimals, char dec_point, char thousands_sep)
	{
		return _php_math_number_format(num, decimals, dec_point, thousands_sep);
	}

	int extension_startup(int type, int module_number)
	{
		zend_module_entry* module;
		void* ptr;
		ZEND_HASH_FOREACH_PTR(&module_registry, ptr)
		{
			module = (zend_module_entry*)ptr;
			if (module_number == module->module_number)
			{
				Extension* extension = _name_to_extension[module->name];
				extension->started = true;
				extension->registerIniEntries(module_number);
				if (extension->onStart)
				{
					extension->onStart();
				}
				_module_number_to_extension[module_number] = extension;
				break;
			}
		}
		ZEND_HASH_FOREACH_END();
		return SUCCESS;
	}

	void extension_info(zend_module_entry* module)
	{
		Extension* extension = _module_number_to_extension[module->module_number];
		if (extension->header.size() > 0 && extension->body.size() > 0)
		{
			php_info_print_table_start();
			auto header = extension->header;
			size_t size = header.size();
			switch (size)
			{
			case 2:
				php_info_print_table_header(size, header[0].c_str(), header[1].c_str());
				break;
			case 3:
				php_info_print_table_header(size, header[0].c_str(), header[1].c_str(), header[2].c_str());
				break;
			default:
				error(E_WARNING, "invalid info header size.");
				return;
			}
			for (auto row : extension->body)
			{
				size = row.size();
				switch (size)
				{
				case 2:
					php_info_print_table_row(size, row[0].c_str(), row[1].c_str());
					break;
				case 3:
					php_info_print_table_row(size, row[0].c_str(), row[1].c_str(), row[2].c_str());
					break;
				default:
					error(E_WARNING, "invalid info row size.");
					return;
				}
			}
			php_info_print_table_end();
		}
	}

	int extension_shutdown(int type, int module_number)
	{
		Extension* extension = _module_number_to_extension[module_number];
		if (extension->onShutdown)
		{
			extension->onShutdown();
		}
		extension->unregisterIniEntries(module_number);
		_name_to_extension.erase(extension->name);
		_module_number_to_extension.erase(module_number);
		delete extension;

		return SUCCESS;
	}

	int extension_before_request(int type, int module_number)
	{
		Extension* extension = _module_number_to_extension[module_number];
		if (extension->onBeforeRequest)
		{
			extension->onBeforeRequest();
		}

		return SUCCESS;
	}

	int extension_after_request(int type, int module_number)
	{
		Extension* extension = _module_number_to_extension[module_number];
		if (extension->onAfterRequest)
		{
			extension->onAfterRequest();
		}

		return SUCCESS;
	}

	static inline ZEND_RESULT_CODE _check_args_num(zend_execute_data* data, int num_args)
	{
		uint32_t min_num_args = data->func->common.required_num_args;
		uint32_t max_num_args = data->func->common.num_args;

		if (num_args < min_num_args || (num_args > max_num_args && max_num_args > 0))
		{
#if PHP_MINOR_VERSION == 0
			zend_wrong_paramers_count_error(num_args, min_num_args, max_num_args);
#elif PHP_MINOR_VERSION == 1
			zend_wrong_parameters_count_error(num_args, min_num_args, max_num_args);
#elif PHP_MINOR_VERSION == 2
			zend_wrong_parameters_count_error(1, num_args, min_num_args, max_num_args);
#else
			zend_wrong_parameters_count_error(min_num_args, max_num_args);
#endif
			return FAILURE;
		}

		return SUCCESS;
	}

	void _exec_function(zend_execute_data* data, zval* return_value)
	{
		function_t func = function_map[(const char*)data->func->common.function_name->val];
		Args args;

		zval* param_ptr = ZEND_CALL_ARG(EG(current_execute_data), 1);
		int arg_count = ZEND_CALL_NUM_ARGS(EG(current_execute_data));

		if (_check_args_num(data, arg_count) == FAILURE)
		{
			return;
		}

		while (arg_count-- > 0)
		{
			args.append(param_ptr);
			param_ptr++;
		}
		Variant _retval(return_value, true);
		func(args, _retval);
	}

	void _exec_method(zend_execute_data* data, zval* return_value)
	{
		method_t func = method_map[(const char*)data->func->common.scope->name->val][(const char*)data->func->common.function_name->val];
		Args args;

		Object _this(&data->This, true);

		zval* param_ptr = ZEND_CALL_ARG(EG(current_execute_data), 1);
		int arg_count = ZEND_CALL_NUM_ARGS(EG(current_execute_data));

		if (_check_args_num(data, arg_count) == FAILURE)
		{
			return;
		}

		while (arg_count-- > 0)
		{
			args.append(param_ptr);
			param_ptr++;
		}
		Variant _retval(return_value, true);
		func(_this, args, _retval);
	}

	Variant _call(zval* object, zval* func, Args& args)
	{
		Variant retval;
		zval params[PHPX_MAX_ARGC];
		for (int i = 0; i < args.count(); i++)
		{
			ZVAL_COPY_VALUE(&params[i], args[i].ptr());
		}
		if (call_user_function(EG(function_table), object, func, retval.ptr(), args.count(), params) == SUCCESS)
		{
			return retval;
		}
		else
		{
			return nullptr;
		}
	}

	Variant _call(zval* object, zval* func)
	{
		Variant retval = false;
		if (call_user_function(EG(function_table), object, func, retval.ptr(), 0, NULL) == 0)
		{
			return retval;
		}
		else
		{
			return nullptr;
		}
	}

	Variant include(string file)
	{
		zend_file_handle file_handle;
		int ret = php_stream_open_for_zend_ex(file.c_str(), &file_handle, USE_PATH | STREAM_OPEN_FOR_INCLUDE);
		if (ret != SUCCESS)
		{
			return false;
		}

		zend_string* opened_path;
		if (!file_handle.opened_path)
		{
			file_handle.opened_path = zend_string_init(file.c_str(), file.length(), 0);
		}
		opened_path = zend_string_copy(file_handle.opened_path);
		zval dummy;
		Variant retval = false;
		zend_op_array* new_op_array;
		ZVAL_NULL(&dummy);
		if (zend_hash_add(&EG(included_files), opened_path, &dummy))
		{
			new_op_array = zend_compile_file(&file_handle, ZEND_REQUIRE);
			zend_destroy_file_handle(&file_handle);
		}
		else
		{
			new_op_array = NULL;
			zend_file_handle_dtor(&file_handle);
		}
		zend_string_release(opened_path);
		if (!new_op_array)
		{
			return false;
		}

		ZVAL_UNDEF(retval.ptr());
		zend_execute(new_op_array, retval.ptr());

		destroy_op_array(new_op_array);
		efree(new_op_array);
		return retval;
	}


	Class::Class(const char* name)
	{
		class_name = name;
		INIT_CLASS_ENTRY_EX(_ce, name, strlen(name), NULL);
		parent_ce = NULL;
		ce = NULL;
		activated = false;
	}

	bool Class::extends(zend_class_entry* _parent_class)
	{
		if (activated)
		{
			return false;
		}
		parent_class_name = string(_parent_class->name->val, _parent_class->name->len);
		parent_ce = _parent_class;
		return parent_ce != NULL;
	}

	bool Class::extends(Class* parent)
	{
		if (activated)
		{
			return false;
		}
		parent_class_name = parent->getName();
		parent_ce = parent->ptr();
		return parent_ce != NULL;
	}

	bool Class::implements(const char* name)
	{
		if (activated)
		{
			return false;
		}
		if (interfaces.find(name) != interfaces.end())
		{
			return false;
		}
		zend_class_entry* interface_ce = getClassEntry(name);
		if (interface_ce == NULL)
		{
			return false;
		}
		interfaces[name] = interface_ce;
		return true;
	}

	bool Class::implements(zend_class_entry* interface_ce)
	{
		if (activated)
		{
			return false;
		}
		interfaces[interface_ce->name->val] = interface_ce;
		return true;
	}

	bool Class::addConstant(const char* name, Variant v)
	{
		if (activated)
		{
			return false;
		}
		Constant c;
		c.name = name;
		ZVAL_COPY(&c.value, v.ptr());
		constants.push_back(c);
		return true;
	}

	bool Class::addProperty(const char* name, Variant v, int flags)
	{
		if (activated)
		{
			return false;
		}
		Property p;
		p.name = name;
		ZVAL_COPY(&p.value, v.ptr());
		p.flags = flags;
		propertys.push_back(p);
		return true;
	}

	bool Class::addMethod(const char* name, method_t method, int flags, ArgInfo* info)
	{
		if (activated)
		{
			return false;
		}
		if ((flags & CONSTRUCT) || (flags & DESTRUCT) || !(flags & ZEND_ACC_PPP_MASK))
		{
			flags |= PUBLIC;
		}
		Method m;
		m.flags = flags;
		m.method = method;
		m.name = name;
		m.info = info;
		methods.push_back(m);
		return false;
	}

	bool Class::alias(const char* alias_name)
	{
		if (activated)
		{
			error(E_WARNING, "Please execute alias method before activate.");
			return false;
		}
		aliases.push_back(alias_name);
		return true;
	}

	bool Class::activate()
	{
		if (activated)
		{
			return false;
		}
		/**
		 * register methods
		 */
		int n = methods.size();
		zend_function_entry* _methods = (zend_function_entry*)ecalloc(n + 1, sizeof(zend_function_entry));
		for (int i = 0; i < n; i++)
		{
			_methods[i].fname = methods[i].name.c_str();
			_methods[i].handler = _exec_method;
			if (methods[i].info)
			{
				_methods[i].arg_info = methods[i].info->get();
				_methods[i].num_args = methods[i].info->count();
			}
			else
			{
				_methods[i].arg_info = nullptr;
				_methods[i].num_args = 0;
			}
			_methods[i].flags = methods[i].flags;
			method_map[class_name.c_str()][methods[i].name.c_str()] = methods[i].method;
		}
		memset(&_methods[n], 0, sizeof(zend_function_entry));
		_ce.info.internal.builtin_functions = _methods;
		if (parent_ce)
		{
			ce = zend_register_internal_class_ex(&_ce, parent_ce);
		}
		else
		{
			ce = zend_register_internal_class(&_ce TSRMLS_CC);
		}
		efree(_methods);
		if (ce == NULL)
		{
			return false;
		}
		/**
		 * implements interface
		 */
		for (auto i = interfaces.begin(); i != interfaces.end(); i++)
		{
			zend_do_implement_interface(ce, interfaces[i->first]);
		}
		/**
		 * register property
		 */
		for (int i = 0; i != propertys.size(); i++)
		{
			Property p = propertys[i];
			if (Z_TYPE(p.value) == IS_STRING)
			{
				zend_declare_property_stringl(ce, p.name.c_str(), p.name.length(), Z_STRVAL(p.value), Z_STRLEN(p.value), p.flags);
			}
			else
			{
				zend_declare_property(ce, p.name.c_str(), p.name.length(), &p.value, p.flags);
			}
		}
		/**
		 * register constant
		 */
		for (int i = 0; i != constants.size(); i++)
		{
			if (Z_TYPE(constants[i].value) == IS_STRING)
			{
				zend_declare_class_constant_stringl(ce, constants[i].name.c_str(), constants[i].name.length(),
					Z_STRVAL(constants[i].value), Z_STRLEN(constants[i].value));
			}
			else
			{
				zend_declare_class_constant(ce, constants[i].name.c_str(), constants[i].name.length(), &constants[i].value);
			}
		}
		for (int i = 0; i < aliases.size(); i++)
		{
			string alias = aliases[i];
#if PHP_VERSION_ID > 70300
			if (zend_register_class_alias_ex(alias.c_str(), alias.length(), ce, 1) < 0)
#else
			if (zend_register_class_alias_ex(alias.c_str(), alias.length(), ce) < 0)
#endif
			{
				return false;
			}
		}
		activated = true;
		return true;
	}


	/*generator*/
	Variant exec(const char* func, const Variant& v1)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		return _call(NULL, _func.ptr(), args);
	}

	Variant exec(const char* func, const Variant& v1, const Variant& v2)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		return _call(NULL, _func.ptr(), args);
	}

	Variant exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		return _call(NULL, _func.ptr(), args);
	}

	Variant exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		return _call(NULL, _func.ptr(), args);
	}

	Variant exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		return _call(NULL, _func.ptr(), args);
	}

	Variant exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		return _call(NULL, _func.ptr(), args);
	}

	Variant exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6, const Variant& v7)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		args.append(const_cast<Variant&>(v7).ptr());
		return _call(NULL, _func.ptr(), args);
	}

	Variant exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6, const Variant& v7, const Variant& v8)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		args.append(const_cast<Variant&>(v7).ptr());
		args.append(const_cast<Variant&>(v8).ptr());
		return _call(NULL, _func.ptr(), args);
	}

	Variant exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6, const Variant& v7, const Variant& v8, const Variant& v9)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		args.append(const_cast<Variant&>(v7).ptr());
		args.append(const_cast<Variant&>(v8).ptr());
		args.append(const_cast<Variant&>(v9).ptr());
		return _call(NULL, _func.ptr(), args);
	}

	Variant exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6, const Variant& v7, const Variant& v8, const Variant& v9, const Variant& v10)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		args.append(const_cast<Variant&>(v7).ptr());
		args.append(const_cast<Variant&>(v8).ptr());
		args.append(const_cast<Variant&>(v9).ptr());
		args.append(const_cast<Variant&>(v10).ptr());
		return _call(NULL, _func.ptr(), args);
	}
	/*generator*/


	Extension::Extension(const char* name, const char* version)
	{
		module.name = name;
		module.version = version;
		this->name = name;
		this->version = version;
		_name_to_extension[name] = this;
	}

	bool Extension::require(const char* name, const char* version)
	{
		this->checkStartupStatus(BEFORE_START, __func__);
		if (module.deps == NULL)
		{
			module.deps = (const zend_module_dep*)calloc(16, sizeof(zend_module_dep));
			if (module.deps == NULL)
			{
				return false;
			}
			deps_array_size = 16;
		}
		else if (deps_count + 1 == deps_array_size)
		{
			deps_array_size *= 2;
			void* new_array = realloc((void*)module.deps, deps_array_size * sizeof(zend_module_dep));
			if (new_array == NULL)
			{
				return false;
			}
			module.deps = (const zend_module_dep*)new_array;
		}

		zend_module_dep* deps_array = (zend_module_dep*)module.deps;
		deps_array[deps_count].name = name;
		deps_array[deps_count].rel = NULL;
		deps_array[deps_count].version = version;
		deps_array[deps_count].type = MODULE_DEP_REQUIRED;

		deps_array[deps_count + 1].name = NULL;
		deps_array[deps_count + 1].rel = NULL;
		deps_array[deps_count + 1].version = NULL;
		deps_array[deps_count + 1].type = 0;

		deps_count++;
		return true;
	}

	bool Extension::registerClass(Class* c)
	{
		this->checkStartupStatus(AFTER_START, __func__);
		c->activate();
		class_map[c->getName()] = c;
		return true;
	}

	bool Extension::registerInterface(Interface* i)
	{
		this->checkStartupStatus(AFTER_START, __func__);
		i->activate();
		interface_map[i->getName()] = i;
		return true;
	}

	bool Extension::registerResource(const char* name, resource_dtor dtor)
	{
		this->checkStartupStatus(AFTER_START, __func__);
		Resource* res = new Resource;
		int type = zend_register_list_destructors_ex(dtor, NULL, name, 0);
		if (type < 0)
		{
			return false;
		}
		res->type = type;
		res->name = name;
		resource_map[name] = res;
		return true;
	}

	void Extension::registerConstant(const char* name, long v)
	{
		zend_register_long_constant(name, strlen(name), v, CONST_CS | CONST_PERSISTENT, module.module_number);
	}

	void Extension::registerConstant(const char* name, int v)
	{
		zend_register_long_constant(name, strlen(name), v, CONST_CS | CONST_PERSISTENT, module.module_number);
	}

	void Extension::registerConstant(const char* name, bool v)
	{
		zend_register_bool_constant(name, strlen(name), v, CONST_CS | CONST_PERSISTENT, module.module_number);
	}

	void Extension::registerConstant(const char* name, const char* v)
	{
		zend_register_string_constant(name, strlen(name), (char*)v, CONST_CS | CONST_PERSISTENT, module.module_number);
	}

	void Extension::registerConstant(const char* name, const char* v, size_t len)
	{
		zend_register_stringl_constant(name, strlen(name), (char*)v, len, CONST_CS | CONST_PERSISTENT,
			module.module_number);
	}

	void Extension::registerConstant(const char* name, double v)
	{
		zend_register_double_constant(name, strlen(name), v, CONST_CS | CONST_PERSISTENT, module.module_number);
	}

	void Extension::registerConstant(const char* name, float v)
	{
		zend_register_double_constant(name, strlen(name), v, CONST_CS | CONST_PERSISTENT, module.module_number);
	}

	void Extension::registerConstant(const char* name, string& v)
	{
		zend_register_stringl_constant(name, strlen(name), (char*)v.c_str(), v.length(), CONST_CS | CONST_PERSISTENT, module.module_number);
	}

	bool Extension::registerFunction(const char* name, function_t func, ArgInfo* info)
	{
		this->checkStartupStatus(BEFORE_START, __func__);
		if (module.functions == NULL)
		{
			module.functions = (const zend_function_entry*)calloc(16, sizeof(zend_function_entry));
			if (module.functions == NULL)
			{
				return false;
			}
			function_array_size = 16;
		}
		else if (function_count + 1 == function_array_size)
		{
			function_array_size *= 2;
			void* new_array = realloc((void*)module.functions, function_array_size * sizeof(zend_function_entry));
			if (new_array == NULL)
			{
				return false;
			}
			module.functions = (const zend_function_entry*)new_array;
		}

		zend_function_entry* function_array = (zend_function_entry*)module.functions;
		function_array[function_count].fname = name;

		function_array[function_count].handler = _exec_function;
		function_array[function_count].arg_info = NULL;
		function_array[function_count].num_args = 0;
		function_array[function_count].flags = 0;
		if (info)
		{
			function_array[function_count].arg_info = info->get();
			function_array[function_count].num_args = info->count();
		}
		else
		{
			function_array[function_count].arg_info = NULL;
			function_array[function_count].num_args = 0;
		}

		function_array[function_count + 1].fname = NULL;
		function_array[function_count + 1].handler = NULL;
		function_array[function_count + 1].flags = 0;

		function_map[name] = func;

		function_count++;
		return true;
	}

	void Extension::registerIniEntries(int module_number) {
		if (!ini_entries.size()) {
			return;
		}

		zend_ini_entry_def* entry_defs = new zend_ini_entry_def[ini_entries.size() + 1];

		for (auto i = 0; i < ini_entries.size(); ++i) {
			IniEntry& entry = ini_entries[i];
			zend_ini_entry_def def = {
					entry.name.c_str(), // name
					NULL,   // on_modify
					NULL,   // mh_arg1
					NULL,   // mh_arg2
					NULL,   // mh_arg3
					entry.default_value.c_str(), // value
					NULL,   // displayer
					entry.modifiable, // modifiable
					(uint)entry.name.size(), // name_length
					(uint)entry.default_value.size(), // value_length
			};
			entry_defs[i] = def;
		}
		memset(entry_defs + ini_entries.size(), 0, sizeof(*entry_defs));

		zend_register_ini_entries(entry_defs, module_number);
		delete[]entry_defs;
	}

	void Extension::unregisterIniEntries(int module_number) {
		if (ini_entries.size()) {
			zend_unregister_ini_entries(module_number);
		}
	}


	Variant http_build_query(const Variant& data, const char* prefix, const char* arg_sep, int enc_type)
	{
		smart_str formstr =
		{ 0 };

		Variant& _data = const_cast<Variant&>(data);
		if (!_data.isArray() && !_data.isObject())
		{
			error(E_WARNING, "Parameter 1 expected to be Array or Object.  Incorrect value given");
			return false;
		}

		size_t prefix_len = prefix != nullptr ? strlen(prefix) : 0;
		if (php_url_encode_hash_ex(HASH_OF(_data.ptr()), &formstr, prefix, prefix_len, NULL, 0, NULL, 0,
			(_data.isObject() ? _data.ptr() : NULL), (char*)arg_sep, enc_type) == FAILURE)
		{
			if (formstr.s)
			{
				smart_str_free(&formstr);
			}
			return false;
		}

		if (!formstr.s)
		{
			return "";
		}

		smart_str_0(&formstr);
		return formstr.s;
	}

	static struct HashAlgo
	{
		const php_hash_ops* md5 = nullptr;
		const php_hash_ops* sha1 = nullptr;
		const php_hash_ops* crc32 = nullptr;
		void* context = nullptr;
		size_t context_size = 0;
		uchar* key = nullptr;
		size_t key_size = 0;
	} hash_algos;

	static String doHash(const php_hash_ops* ops, String& data, bool raw_output)
	{
		if (hash_algos.context_size < ops->context_size)
		{
			hash_algos.context_size = ops->context_size;
			hash_algos.context = malloc(hash_algos.context_size);
		}

		void* context = hash_algos.context;
		ops->hash_init(context);
		ops->hash_update(context, (unsigned char*)data.c_str(), data.length());

		zend_string* digest = zend_string_alloc(ops->digest_size, 0);
		ops->hash_final((unsigned char*)ZSTR_VAL(digest), context);

		if (raw_output)
		{
			ZSTR_VAL(digest)[ops->digest_size] = 0;
			return digest;
		}
		else
		{
			zend_string* hex_digest = zend_string_safe_alloc(ops->digest_size, 2, 0, 0);
			php_hash_bin2hex(ZSTR_VAL(hex_digest), (unsigned char*)ZSTR_VAL(digest), ops->digest_size);
			ZSTR_VAL(hex_digest)[2 * ops->digest_size] = 0;
			zend_string_release(digest);
			return hex_digest;
		}
	}

	String md5(String data, bool raw_output)
	{
		if (hash_algos.md5 == nullptr)
		{
			hash_algos.md5 = php_hash_fetch_ops(ZEND_STRL("md5"));
		}
		return doHash(hash_algos.md5, data, raw_output);
	}

	String sha1(String data, bool raw_output)
	{
		if (hash_algos.sha1 == nullptr)
		{
			hash_algos.sha1 = php_hash_fetch_ops(ZEND_STRL("sha1"));
		}
		return doHash(hash_algos.sha1, data, raw_output);
	}

	String crc32(String data, bool raw_output)
	{
		if (hash_algos.crc32 == nullptr)
		{
			hash_algos.crc32 = php_hash_fetch_ops(ZEND_STRL("crc32"));
		}
		return doHash(hash_algos.crc32, data, raw_output);
	}

	String hash(String algo, String data, bool raw_output)
	{
		const php_hash_ops* ops = php_hash_fetch_ops(algo.c_str(), algo.length());
		if (!ops)
		{
			return "";
		}
		return doHash(ops, data, raw_output);
	}

	static inline void php_hash_string_xor_char(uchar* out, const uchar* in, const uchar xor_with, const int length)
	{
		int i;
		for (i = 0; i < length; i++)
		{
			out[i] = in[i] ^ xor_with;
		}
	}

	static inline void php_hash_hmac_prep_key(uchar* K, const php_hash_ops* ops, void* context, const uchar* key, const size_t key_len)
	{
		memset(K, 0, ops->block_size);
		if (key_len > ops->block_size)
		{
			/* Reduce the key first */
			ops->hash_init(context);
			ops->hash_update(context, key, key_len);
			ops->hash_final(K, context);
		}
		else
		{
			memcpy(K, key, key_len);
		}
		/* XOR the key with 0x36 to get the ipad) */
		php_hash_string_xor_char(K, K, 0x36, ops->block_size);
	}

	String hash_hmac(String algo, String data, String key, bool raw_output)
	{
		const php_hash_ops* ops = php_hash_fetch_ops(algo.c_str(), algo.length());
		if (!ops)
		{
			return "";
		}
		if (hash_algos.context_size < ops->context_size)
		{
			hash_algos.context_size = ops->context_size;
			hash_algos.context = malloc(hash_algos.context_size);
		}
		if (hash_algos.key_size < ops->block_size)
		{
			hash_algos.key_size = ops->block_size;
			hash_algos.key = (uchar*)malloc(hash_algos.key_size);
		}

		void* context = hash_algos.context;
		uchar* _key = hash_algos.key;
		zend_string* digest = zend_string_alloc(ops->digest_size, 0);

		php_hash_hmac_prep_key((uchar*)_key, ops, context, (uchar*)key.c_str(), key.length());

		ops->hash_init(context);
		ops->hash_update(context, _key, ops->block_size);
		ops->hash_update(context, (uchar*)data.c_str(), data.length());
		ops->hash_final((uchar*)ZSTR_VAL(digest), context);

		php_hash_string_xor_char(_key, _key, 0x6A, ops->block_size);

		ops->hash_init(context);
		ops->hash_update(context, _key, ops->block_size);
		ops->hash_update(context, (uchar*)ZSTR_VAL(digest), ops->digest_size);
		ops->hash_final((uchar*)ZSTR_VAL(digest), context);

		if (raw_output)
		{
			ZSTR_VAL(digest)[ops->digest_size] = 0;
			return digest;
		}
		else
		{
			zend_string* hex_digest = zend_string_safe_alloc(ops->digest_size, 2, 0, 0);
			php_hash_bin2hex(ZSTR_VAL(hex_digest), (uchar*)ZSTR_VAL(digest), ops->digest_size);
			ZSTR_VAL(hex_digest)[2 * ops->digest_size] = 0;
			zend_string_release(digest);
			return hex_digest;
		}
	}

	Object newObject(const char* name)
	{
		Object object;
		zend_class_entry* ce = getClassEntry(name);
		if (ce == NULL)
		{
			error(E_WARNING, "class '%s' is undefined.", name);
			return object;
		}
		if (object_init_ex(object.ptr(), ce) == FAILURE)
		{
			return object;
		}
		Args args;
		object.call("__construct", args);
		return object;
	}

	/*generator-1*/
	Variant Object::exec(const char* func, const Variant& v1)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		return _call(ptr(), _func.ptr(), args);
	}

	Variant Object::exec(const char* func, const Variant& v1, const Variant& v2)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		return _call(ptr(), _func.ptr(), args);
	}

	Variant Object::exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		return _call(ptr(), _func.ptr(), args);
	}

	Variant Object::exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		return _call(ptr(), _func.ptr(), args);
	}

	Variant Object::exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		return _call(ptr(), _func.ptr(), args);
	}

	Variant Object::exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		return _call(ptr(), _func.ptr(), args);
	}

	Variant Object::exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6, const Variant& v7)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		args.append(const_cast<Variant&>(v7).ptr());
		return _call(ptr(), _func.ptr(), args);
	}

	Variant Object::exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6, const Variant& v7, const Variant& v8)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		args.append(const_cast<Variant&>(v7).ptr());
		args.append(const_cast<Variant&>(v8).ptr());
		return _call(ptr(), _func.ptr(), args);
	}

	Variant Object::exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6, const Variant& v7, const Variant& v8, const Variant& v9)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		args.append(const_cast<Variant&>(v7).ptr());
		args.append(const_cast<Variant&>(v8).ptr());
		args.append(const_cast<Variant&>(v9).ptr());
		return _call(ptr(), _func.ptr(), args);
	}

	Variant Object::exec(const char* func, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6, const Variant& v7, const Variant& v8, const Variant& v9, const Variant& v10)
	{
		Variant _func(func);
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		args.append(const_cast<Variant&>(v7).ptr());
		args.append(const_cast<Variant&>(v8).ptr());
		args.append(const_cast<Variant&>(v9).ptr());
		args.append(const_cast<Variant&>(v10).ptr());
		return _call(ptr(), _func.ptr(), args);
	}
	/*generator-1*/

	/*generator*/
	Object newObject(const char* name, const Variant& v1)
	{
		Object object;
		zend_class_entry* ce = getClassEntry(name);
		if (ce == NULL)
		{
			error(E_WARNING, "class '%s' is undefined.", name);
			return object;
		}
		if (object_init_ex(object.ptr(), ce) == FAILURE)
		{
			return object;
		}
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		object.call("__construct", args);
		return object;
	}

	Object newObject(const char* name, const Variant& v1, const Variant& v2)
	{
		Object object;
		zend_class_entry* ce = getClassEntry(name);
		if (ce == NULL)
		{
			error(E_WARNING, "class '%s' is undefined.", name);
			return object;
		}
		if (object_init_ex(object.ptr(), ce) == FAILURE)
		{
			return object;
		}
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		object.call("__construct", args);
		return object;
	}

	Object newObject(const char* name, const Variant& v1, const Variant& v2, const Variant& v3)
	{
		Object object;
		zend_class_entry* ce = getClassEntry(name);
		if (ce == NULL)
		{
			error(E_WARNING, "class '%s' is undefined.", name);
			return object;
		}
		if (object_init_ex(object.ptr(), ce) == FAILURE)
		{
			return object;
		}
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		object.call("__construct", args);
		return object;
	}

	Object newObject(const char* name, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4)
	{
		Object object;
		zend_class_entry* ce = getClassEntry(name);
		if (ce == NULL)
		{
			error(E_WARNING, "class '%s' is undefined.", name);
			return object;
		}
		if (object_init_ex(object.ptr(), ce) == FAILURE)
		{
			return object;
		}
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		object.call("__construct", args);
		return object;
	}

	Object newObject(const char* name, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5)
	{
		Object object;
		zend_class_entry* ce = getClassEntry(name);
		if (ce == NULL)
		{
			error(E_WARNING, "class '%s' is undefined.", name);
			return object;
		}
		if (object_init_ex(object.ptr(), ce) == FAILURE)
		{
			return object;
		}
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		object.call("__construct", args);
		return object;
	}

	Object newObject(const char* name, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6)
	{
		Object object;
		zend_class_entry* ce = getClassEntry(name);
		if (ce == NULL)
		{
			error(E_WARNING, "class '%s' is undefined.", name);
			return object;
		}
		if (object_init_ex(object.ptr(), ce) == FAILURE)
		{
			return object;
		}
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		object.call("__construct", args);
		return object;
	}

	Object newObject(const char* name, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6, const Variant& v7)
	{
		Object object;
		zend_class_entry* ce = getClassEntry(name);
		if (ce == NULL)
		{
			error(E_WARNING, "class '%s' is undefined.", name);
			return object;
		}
		if (object_init_ex(object.ptr(), ce) == FAILURE)
		{
			return object;
		}
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		args.append(const_cast<Variant&>(v7).ptr());
		object.call("__construct", args);
		return object;
	}

	Object newObject(const char* name, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6, const Variant& v7, const Variant& v8)
	{
		Object object;
		zend_class_entry* ce = getClassEntry(name);
		if (ce == NULL)
		{
			error(E_WARNING, "class '%s' is undefined.", name);
			return object;
		}
		if (object_init_ex(object.ptr(), ce) == FAILURE)
		{
			return object;
		}
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		args.append(const_cast<Variant&>(v7).ptr());
		args.append(const_cast<Variant&>(v8).ptr());
		object.call("__construct", args);
		return object;
	}

	Object newObject(const char* name, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6, const Variant& v7, const Variant& v8, const Variant& v9)
	{
		Object object;
		zend_class_entry* ce = getClassEntry(name);
		if (ce == NULL)
		{
			error(E_WARNING, "class '%s' is undefined.", name);
			return object;
		}
		if (object_init_ex(object.ptr(), ce) == FAILURE)
		{
			return object;
		}
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		args.append(const_cast<Variant&>(v7).ptr());
		args.append(const_cast<Variant&>(v8).ptr());
		args.append(const_cast<Variant&>(v9).ptr());
		object.call("__construct", args);
		return object;
	}

	Object newObject(const char* name, const Variant& v1, const Variant& v2, const Variant& v3, const Variant& v4, const Variant& v5, const Variant& v6, const Variant& v7, const Variant& v8, const Variant& v9, const Variant& v10)
	{
		Object object;
		zend_class_entry* ce = getClassEntry(name);
		if (ce == NULL)
		{
			error(E_WARNING, "class '%s' is undefined.", name);
			return object;
		}
		if (object_init_ex(object.ptr(), ce) == FAILURE)
		{
			return object;
		}
		Args args;
		args.append(const_cast<Variant&>(v1).ptr());
		args.append(const_cast<Variant&>(v2).ptr());
		args.append(const_cast<Variant&>(v3).ptr());
		args.append(const_cast<Variant&>(v4).ptr());
		args.append(const_cast<Variant&>(v5).ptr());
		args.append(const_cast<Variant&>(v6).ptr());
		args.append(const_cast<Variant&>(v7).ptr());
		args.append(const_cast<Variant&>(v8).ptr());
		args.append(const_cast<Variant&>(v9).ptr());
		args.append(const_cast<Variant&>(v10).ptr());
		object.call("__construct", args);
		return object;
	}
	/*generator*/

	String String::substr(long _offset, long _length)
	{

		if ((_length < 0 && (size_t)(-_length) > this->length()))
		{
			return "";
		}
		else if (_length > (zend_long)this->length())
		{
			_length = this->length();
		}

		if (_offset > (zend_long)this->length())
		{
			return "";
		}
		else if (_offset < 0 && -_offset > this->length())
		{
			_offset = 0;
		}

		if (_length < 0 && (_length + (zend_long)this->length() - _offset) < 0)
		{
			return "";
		}

		/* if "from" position is negative, count start position from the end
		 * of the string
		 */
		if (_offset < 0)
		{
			_offset = (zend_long)this->length() + _offset;
			if (_offset < 0)
			{
				_offset = 0;
			}
		}

		/* if "length" position is negative, set it to the length
		 * needed to stop that many chars from the end of the string
		 */
		if (_length < 0)
		{
			_length = ((zend_long)this->length() - _offset) + _length;
			if (_length < 0)
			{
				_length = 0;
			}
		}

		if (_offset > (zend_long) this->length())
		{
			return "";
		}

		if ((_offset + _length) > (zend_long)this->length())
		{
			_length = this->length() - _offset;
		}

		return String(value->val + _offset, _length);
	}

	Variant String::split(String& delim, long limit)
	{
		Array retval;
		php_explode(delim.ptr(), value, retval.ptr(), limit);
		return retval;
	}

	void String::stripTags(String& allow, bool allow_tag_spaces)
	{
		value->len = php_strip_tags_ex(this->c_str(), this->length(), nullptr, allow.c_str(), allow.length(), allow_tag_spaces);
	}

	String String::addSlashes()
	{
#if PHP_VERSION_ID > 70300
		return php_addslashes(value);
#else
		return php_addslashes(value, false);
#endif
	}

	String String::basename(String& suffix)
	{
		return php_basename(this->c_str(), this->length(), suffix.c_str(), suffix.length());
	}

	String String::dirname()
	{
		size_t n = php_dirname(this->c_str(), this->length());
		return String(this->c_str(), n);
	}

	void String::stripSlashes()
	{
		php_stripslashes(value);
	}


	bool Variant::equals(Variant& v, bool strict)
	{
		if (strict)
		{
			if (fast_is_identical_function(v.ptr(), ptr()))
			{
				return true;
			}
		}
		else
		{
			if (v.isInt())
			{
				if (fast_equal_check_long(v.ptr(), ptr()))
				{
					return true;
				}
			}
			else if (v.isString())
			{
				if (fast_equal_check_string(v.ptr(), ptr()))
				{
					return true;
				}
			}
			else
			{
				if (fast_equal_check_function(v.ptr(), ptr()))
				{
					return true;

				}
			}
		}
		return false;
	}

	Variant Variant::serialize()
	{
		smart_str serialized_data = { 0 };
		php_serialize_data_t var_hash;
		PHP_VAR_SERIALIZE_INIT(var_hash);
		php_var_serialize(&serialized_data, ptr(), &var_hash TSRMLS_CC);
		PHP_VAR_SERIALIZE_DESTROY(var_hash);
		Variant retval(serialized_data.s->val, serialized_data.s->len);
		smart_str_free(&serialized_data);
		return retval;
	}

	Variant Variant::unserialize()
	{
		php_unserialize_data_t var_hash;
		Variant retval;
		PHP_VAR_UNSERIALIZE_INIT(var_hash);

		char* data = Z_STRVAL_P(ptr());
		size_t length = Z_STRLEN_P(ptr());
		if (php_var_unserialize(retval.ptr(), (const uchar * *)& data, (const uchar*)data + length, &var_hash))
		{
			return retval;
		}
		else
		{
			return nullptr;
		}
	}

	Variant Variant::jsonEncode(zend_long options, zend_long depth)
	{
		smart_str buf = { 0 };
		JSON_G(error_code) = PHP_JSON_ERROR_NONE;
		JSON_G(encode_max_depth) = (int)depth;

		php_json_encode(&buf, ptr(), (int)options);

		if (JSON_G(error_code) != PHP_JSON_ERROR_NONE && !(options & PHP_JSON_PARTIAL_OUTPUT_ON_ERROR))
		{
			smart_str_free(&buf);
			return false;
		}
		else
		{
			smart_str_0(&buf);
			return buf.s;
		}
	}

	Variant Variant::jsonDecode(zend_long options, zend_long depth)
	{
		smart_str buf = { 0 };
		JSON_G(error_code) = PHP_JSON_ERROR_NONE;

		if (this->length() == 0)
		{
			JSON_G(error_code) = PHP_JSON_ERROR_SYNTAX;
			return nullptr;
		}

		options |= PHP_JSON_OBJECT_AS_ARRAY;
		Variant retval;
		php_json_decode_ex(retval.ptr(), Z_STRVAL_P(ptr()), Z_STRLEN_P(ptr()), options, depth);
		return retval;
	}

	bool Variant::isCallable()
	{
		return zend_is_callable(ptr(), 0, nullptr);
	}
}
