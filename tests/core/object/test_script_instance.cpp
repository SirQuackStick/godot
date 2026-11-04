/**************************************************************************/
/*  test_script_instance.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_script_instance)

#include "core/object/method_info.h"
#include "core/object/property_info.h"
#include "core/object/script_instance.h"
#include "core/object/script_language.h"
#include "core/string/string_name.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/list.h"
#include "core/templates/pair.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

namespace TestScriptInstance {

// A script whose base script, methods and RPC config can be configured per test.
// Only what ScriptInstance's default implementations touch is configurable.
class _MockScript final : public Script {
public:
	Ref<Script> base;
	HashMap<StringName, MethodInfo> methods;
	Variant rpc_config;

	virtual bool can_instantiate() const override { return false; }
	virtual Ref<Script> get_base_script() const override { return base; }
	virtual StringName get_global_name() const override { return StringName(); }
	virtual bool inherits_script(const Ref<Script> &p_script) const override { return false; }
	virtual StringName get_instance_base_type() const override { return SNAME("Object"); }
	virtual ScriptInstance *instance_create(Object *p_this) override { return nullptr; }
	virtual bool has_source_code() const override { return false; }
	virtual String get_source_code() const override { return String(); }
	virtual void set_source_code(const String &p_code) override {}
	virtual Error reload(bool p_keep_state = false) override { return OK; }
	virtual StringName get_doc_class_name() const override { return StringName(); }
	virtual Vector<DocData::ClassDoc> get_documentation() const override { return Vector<DocData::ClassDoc>(); }
	virtual String get_class_icon_path() const override { return String(); }
	virtual bool has_method(const StringName &p_method) const override { return methods.has(p_method); }
	virtual MethodInfo get_method_info(const StringName &p_method) const override {
		const MethodInfo *mi = methods.getptr(p_method);
		return mi ? *mi : MethodInfo();
	}
	virtual bool is_tool() const override { return false; }
	virtual bool is_script_valid() const override { return true; }
	virtual bool is_abstract() const override { return false; }
	virtual ScriptLanguage *get_language() const override { return nullptr; }
	virtual bool has_script_signal(const StringName &p_signal) const override { return false; }
	virtual void get_script_signal_list(List<MethodInfo> *r_signals) const override {}
	virtual bool get_property_default_value(const StringName &p_property, Variant &r_value) const override { return false; }
	virtual void get_script_method_list(List<MethodInfo> *r_list) const override {}
	virtual void get_script_property_list(List<PropertyInfo> *r_list) const override {}
	virtual const Variant get_rpc_config() const override { return rpc_config; }
};

// Implements only the pure virtuals of ScriptInstance
// under test runs the base class implementation.
// Do NOT override get_method_argument_count, call_const, get_property_state,
// property_set_fallback, property_get_fallback or get_rpc_config
class _MockScriptInstance final : public ScriptInstance {
public:
	Ref<_MockScript> script;

	// Property storage
	List<PropertyInfo> property_infos;
	HashMap<StringName, Variant> values;
	HashSet<StringName> failing_gets;

	int callp_count = 0;
	StringName last_method;
	Vector<Variant> last_args;
	int last_argcount = -1;
	bool last_args_was_null = false;
	Variant callp_return;
	Callable::CallError::Error callp_error = Callable::CallError::CALL_OK;

	virtual bool set(const StringName &p_name, const Variant &p_value) override {
		values[p_name] = p_value;
		return true;
	}
	virtual bool get(const StringName &p_name, Variant &r_ret) const override {
		if (failing_gets.has(p_name)) {
			return false;
		}
		const Variant *v = values.getptr(p_name);
		if (v == nullptr) {
			return false;
		}
		r_ret = *v;
		return true;
	}
	virtual void get_property_list(List<PropertyInfo> *r_properties) const override {
		for (const PropertyInfo &E : property_infos) {
			r_properties->push_back(E);
		}
	}
	virtual Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid = nullptr) const override {
		if (r_is_valid) {
			*r_is_valid = false;
		}
		return Variant::NIL;
	}
	virtual void validate_property(PropertyInfo &p_property) const override {}
	virtual bool property_can_revert(const StringName &p_name) const override { return false; }
	virtual bool property_get_revert(const StringName &p_name, Variant &r_ret) const override { return false; }
	virtual void get_method_list(List<MethodInfo> *r_list) const override {}
	virtual bool has_method(const StringName &p_method) const override { return false; }
	virtual Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override {
		callp_count++;
		last_method = p_method;
		last_argcount = p_argcount;
		last_args_was_null = p_args == nullptr;
		last_args.clear();
		for (int i = 0; i < p_argcount; i++) {
			last_args.push_back(*p_args[i]);
		}
		r_error.error = callp_error;
		return callp_return;
	}
	virtual void notification(int p_notification, bool p_reversed = false) override {}
	virtual Script *get_script() const override { return script.ptr(); }
	virtual ScriptLanguage *get_language() override { return nullptr; }
};

static Ref<_MockScript> make_script_with_method(const StringName &p_name, int p_argc) {
	Ref<_MockScript> scr;
	scr.instantiate();
	MethodInfo mi(p_name);
	for (int i = 0; i < p_argc; i++) {
		mi.arguments.push_back(PropertyInfo(Variant::INT, String("arg") + itos(i)));
	}
	scr->methods.insert(p_name, mi);
	return scr;
}

TEST_SUITE("[ScriptInstance]") {
	TEST_CASE("get_method_argument_count finds a method on the instance's own script.") {
		_MockScriptInstance inst;
		inst.script = make_script_with_method("foo", 2);

		bool valid = false;
		CHECK_EQ(inst.get_method_argument_count("foo", &valid), 2);
		CHECK(valid);
	}

	TEST_CASE("get_method_argument_count walks up the base script chain.") {
		Ref<_MockScript> root = make_script_with_method("foo", 3);
		Ref<_MockScript> mid;
		mid.instantiate();
		mid->base = root;
		Ref<_MockScript> leaf;
		leaf.instantiate();
		leaf->base = mid;

		_MockScriptInstance inst;
		inst.script = leaf;

		bool valid = false;
		CHECK_EQ(inst.get_method_argument_count("foo", &valid), 3);
		CHECK(valid);
	}

	TEST_CASE("get_method_argument_count uses the nearest script when a method is overridden.") {
		Ref<_MockScript> root = make_script_with_method("foo", 3);
		Ref<_MockScript> leaf = make_script_with_method("foo", 1);
		leaf->base = root;

		_MockScriptInstance inst;
		inst.script = leaf;

		bool valid = false;
		CHECK_EQ(inst.get_method_argument_count("foo", &valid), 1);
		CHECK(valid);
	}

	TEST_CASE("get_method_argument_count reports an invalid result for a missing method.") {
		Ref<_MockScript> root = make_script_with_method("foo", 3);
		Ref<_MockScript> leaf;
		leaf.instantiate();
		leaf->base = root;

		_MockScriptInstance inst;
		inst.script = leaf;

		bool valid = true;
		CHECK_EQ(inst.get_method_argument_count("bar", &valid), 0);
		CHECK_FALSE(valid);
	}

	TEST_CASE("get_method_argument_count handles an instance without a script.") {
		_MockScriptInstance inst;

		bool valid = true;
		CHECK_EQ(inst.get_method_argument_count("foo", &valid), 0);
		CHECK_FALSE(valid);
	}

	TEST_CASE("get_method_argument_count accepts a null r_is_valid.") {
		_MockScriptInstance inst;
		inst.script = make_script_with_method("foo", 2);

		CHECK_EQ(inst.get_method_argument_count("foo"), 2);
		CHECK_EQ(inst.get_method_argument_count("bar"), 0);
	}

	TEST_CASE("get_property_state returns only readable storage properties, in list order.") {
		_MockScriptInstance inst;
		inst.property_infos.push_back(PropertyInfo(Variant::INT, "stored_a", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));
		inst.property_infos.push_back(PropertyInfo(Variant::INT, "editor_only", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR));
		inst.property_infos.push_back(PropertyInfo(Variant::INT, "stored_unreadable", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));
		inst.property_infos.push_back(PropertyInfo(Variant::STRING, "stored_b"));
		inst.values["stored_a"] = 1;
		inst.values["editor_only"] = 2;
		inst.values["stored_unreadable"] = 3;
		inst.values["stored_b"] = "b";
		inst.failing_gets.insert("stored_unreadable");

		List<Pair<StringName, Variant>> state;
		inst.get_property_state(state);

		REQUIRE_EQ(state.size(), 2);
		CHECK(state.front()->get().first == StringName("stored_a"));
		CHECK_EQ(state.front()->get().second, Variant(1));
		CHECK(state.back()->get().first == StringName("stored_b"));
		CHECK_EQ(state.back()->get().second, Variant("b"));
	}

	TEST_CASE("get_property_state appends to an existing list.") {
		_MockScriptInstance inst;
		inst.property_infos.push_back(PropertyInfo(Variant::INT, "stored_a"));
		inst.values["stored_a"] = 1;

		List<Pair<StringName, Variant>> state;
		state.push_back(Pair<StringName, Variant>("pre_existing", 0));
		inst.get_property_state(state);

		REQUIRE_EQ(state.size(), 2);
		CHECK(state.front()->get().first == StringName("pre_existing"));
		CHECK(state.back()->get().first == StringName("stored_a"));
	}

	TEST_CASE("call_const forwards to callp, including arguments and the call error.") {
		_MockScriptInstance inst;
		inst.callp_return = 42;
		inst.callp_error = Callable::CallError::CALL_ERROR_INVALID_METHOD;

		Variant a = 7;
		Variant b = "x";
		const Variant *args[2] = { &a, &b };
		Callable::CallError err;
		Variant ret = inst.call_const("do_thing", args, 2, err);

		CHECK_EQ(inst.callp_count, 1);
		CHECK(inst.last_method == StringName("do_thing"));
		CHECK_EQ(inst.last_argcount, 2);
		REQUIRE_EQ(inst.last_args.size(), 2);
		CHECK_EQ(inst.last_args[0], Variant(7));
		CHECK_EQ(inst.last_args[1], Variant("x"));
		CHECK_EQ(ret, Variant(42));
		CHECK(err.error == Callable::CallError::CALL_ERROR_INVALID_METHOD);
	}

	TEST_CASE("call() with no arguments passes a null argument array to callp.") {
		_MockScriptInstance inst;
		inst.callp_return = "ok";

		Variant ret = inst.call("no_args");

		CHECK_EQ(inst.callp_count, 1);
		CHECK(inst.last_method == StringName("no_args"));
		CHECK_EQ(inst.last_argcount, 0);
		CHECK(inst.last_args_was_null);
		CHECK_EQ(ret, Variant("ok"));
	}

	TEST_CASE("call() packs mixed-type arguments in order.") {
		_MockScriptInstance inst;

		inst.call("mixed", 1, "two", 3.5, Variant());

		CHECK_EQ(inst.last_argcount, 4);
		CHECK_FALSE(inst.last_args_was_null);
		REQUIRE_EQ(inst.last_args.size(), 4);
		CHECK_EQ(inst.last_args[0], Variant(1));
		CHECK_EQ(inst.last_args[1], Variant("two"));
		CHECK_EQ(inst.last_args[2], Variant(3.5));
		CHECK_EQ(inst.last_args[3], Variant());
	}

	TEST_CASE("Property fallbacks report failure by default.") {
		_MockScriptInstance inst;

		bool valid = true;
		inst.property_set_fallback("anything", 1, &valid);
		CHECK_FALSE(valid);

		valid = true;
		CHECK_EQ(inst.property_get_fallback("anything", &valid), Variant());
		CHECK_FALSE(valid);

		inst.property_set_fallback("anything", 1, nullptr);
		CHECK_EQ(inst.property_get_fallback("anything", nullptr), Variant());
	}

	TEST_CASE("get_rpc_config forwards to the script.") {
		Dictionary config;
		config["my_rpc"] = 1;

		_MockScriptInstance inst;
		inst.script.instantiate();
		inst.script->rpc_config = config;

		CHECK_EQ(inst.get_rpc_config(), Variant(config));
	}

	TEST_CASE("Default virtual implementations.") {
		_MockScriptInstance inst;

		CHECK(inst.get_owner() == nullptr);
		CHECK_FALSE(inst.is_placeholder());
		CHECK(inst.refcount_decremented());
		inst.refcount_incremented();

		bool valid = true;
		CHECK_EQ(inst.to_string(&valid), String());
		CHECK_FALSE(valid);
		CHECK_EQ(inst.to_string(nullptr), String());
	}
}

} // namespace TestScriptInstance
