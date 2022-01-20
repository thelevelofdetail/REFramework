#include <deque>
#include <mutex>
#include <shared_mutex>

#include "REFramework.hpp"

#include "RETypeDB.hpp"
#include "RETypeDefinition.hpp"

namespace sdk {
struct RETypeDefinition;

sdk::REMethodDefinition* RETypeDefinition::MethodIterator::begin() const {
    if (m_parent->member_method == 0) {
        return nullptr;
    }

    auto tdb = RETypeDB::get();

    return &(*tdb->methods)[m_parent->member_method];
}

sdk::REMethodDefinition* RETypeDefinition::MethodIterator::end()  const {
    if (m_parent->member_method == 0) {
        return nullptr;
    }

    auto tdb = RETypeDB::get();

#if TDB_VER >= 69
    const auto& impl = (*tdb->typesImpl)[m_parent->impl_index];
    const auto num_methods = impl.num_member_methods;
#else
    const auto num_methods = m_parent->num_member_method;
#endif

    return &(*tdb->methods)[m_parent->member_method + num_methods];
}

size_t RETypeDefinition::MethodIterator::size() const {
    return ((uintptr_t)end() - (uintptr_t)begin()) / sizeof(sdk::REMethodDefinition);
}

sdk::REField* sdk::RETypeDefinition::FieldIterator::REFieldIterator::operator*() const {
//#ifndef RE7
    if (m_parent->member_field == 0) {
        return nullptr;
    }

    auto tdb = RETypeDB::get();

    return &(*tdb->fields)[m_parent->member_field + m_index];
/*#else
    if (m_parent->member_field_start == 0) {
        return nullptr;
    }

    auto tdb = RETypeDB::get();
    const auto field_indices = tdb->get_data<uint32_t>(m_parent->member_field_start);

    const auto index = field_indices[m_index];

    if (index == 0) {
        return nullptr;
    }

    return &(*tdb->fields)[index];
#endif*/
}

size_t sdk::RETypeDefinition::FieldIterator::size() const {
#if TDB_VER >= 69
        auto tdb = sdk::RETypeDB::get();
        const auto& impl = (*tdb->typesImpl)[m_parent->impl_index];
        const auto num_fields = impl.num_member_fields;
#else
        const auto num_fields = m_parent->num_member_field;
#endif

        return num_fields;
    }

sdk::REProperty* RETypeDefinition::PropertyIterator::begin() const {
    if (m_parent->member_prop == 0) {
        return nullptr;
    }

    auto tdb = RETypeDB::get();

    return &(*tdb->properties)[m_parent->member_prop];
}

sdk::REProperty* RETypeDefinition::PropertyIterator::end() const {
    if (m_parent->member_prop == 0) {
        return nullptr;
    }

    auto tdb = RETypeDB::get();

    const auto num_prop = m_parent->num_member_prop;

    return &(*tdb->properties)[m_parent->member_prop + num_prop];
}

size_t RETypeDefinition::PropertyIterator::size() const {
    return ((uintptr_t)end() - (uintptr_t)begin()) / sizeof(sdk::REProperty);
}

const char* RETypeDefinition::get_namespace() const {
    auto tdb = RETypeDB::get();

#if TDB_VER >= 69
    auto& impl = (*tdb->typesImpl)[this->impl_index];

    const auto name_index = impl.namespace_offset;
#else
    const auto name_index = this->namespace_offset;
#endif

    return tdb->get_string(name_index);
}

const char* RETypeDefinition::get_name() const {
    auto tdb = RETypeDB::get();

#if TDB_VER >= 69
    auto& impl = (*tdb->typesImpl)[this->impl_index];

    const auto name_index = impl.name_offset;
#else
    const auto name_index = this->name_offset;
#endif

    return tdb->get_string(name_index);
}

// because who knows where this is going to be called from.
static std::unordered_map<uint32_t, std::string> g_full_names{};
static std::shared_mutex g_full_name_mtx{};

std::string RETypeDefinition::get_full_name() const {
    auto tdb = RETypeDB::get();

#ifdef RE7
    return tdb->get_string(this->full_name_offset); // uhh thanks?
#else
    {
        std::shared_lock _{ g_full_name_mtx };

        if (auto it = g_full_names.find(this->get_index()); it != g_full_names.end()) {
            return it->second;
        }
    }

    std::deque<std::string> names{};
    std::string full_name{};

    if (this->declaring_typeid > 0 && this->declaring_typeid != this->get_index()) {
        std::unordered_set<const sdk::RETypeDefinition*> seen_classes{};

        for (auto owner = this; owner != nullptr; owner = owner->get_declaring_type()) {
            if (seen_classes.count(owner) > 0) {
                break;
            }

            names.push_front(owner->get_name());

            if (owner->get_declaring_type() == nullptr && !std::string{owner->get_namespace()}.empty()) {
                names.push_front(owner->get_namespace());
            }

            // uh.
            if (owner->get_declaring_type() == this) {
                break;
            }

            seen_classes.insert(owner);
        }
    } else {
        // namespace
        if (!std::string{this->get_namespace()}.empty()) {
            names.push_front(this->get_namespace());
        }

        // actual class name
        names.push_back(this->get_name());
    }

    for (auto f = 0; f < names.size(); ++f) {
        if (f > 0) {
            full_name += ".";
        }

        full_name += names[f];
    }

    // Set this here at this point in-case get_full_name runs into it
    {
        std::unique_lock _{g_full_name_mtx};
        g_full_names[this->get_index()] = full_name;
    }

#ifndef RE7
    if (this->generics > 0) {
        auto generics = tdb->get_data<sdk::GenericListData>(this->generics);

        if (generics->num > 0) {
            full_name += "<";

            for (uint32_t f = 0; f < generics->num; ++f) {
                auto gtypeid = generics->types[f];

                if (gtypeid > 0 && gtypeid < tdb->numTypes) {
                    auto& generic_type = *tdb->get_type(gtypeid);
                    full_name += generic_type.get_full_name();
                } else {
                    full_name += "";
                }

                if (generics->num > 1 && f < generics->num - 1) {
                    full_name += ",";
                }
            }

            full_name += ">";
        }
    }
#else
    //full_name += "<not implemented>";
#endif

    {
        std::unique_lock _{g_full_name_mtx};
        g_full_names[this->get_index()] = full_name;
    }

    return full_name;
#endif
}

sdk::RETypeDefinition* RETypeDefinition::get_declaring_type() const {
    auto tdb = RETypeDB::get();

    if (this->declaring_typeid == 0 || this->declaring_typeid >= tdb->numTypes) {
        return nullptr;
    }

    return tdb->get_type(this->declaring_typeid);
}

sdk::RETypeDefinition* RETypeDefinition::get_parent_type() const {
    auto tdb = RETypeDB::get();

    if (this->parent_typeid == 0 || this->parent_typeid >= tdb->numTypes) {
        return nullptr;
    }

    return tdb->get_type(this->parent_typeid);
}

static std::shared_mutex g_underlying_mtx{};
static std::unordered_map<const sdk::RETypeDefinition*, sdk::RETypeDefinition*> g_underlying_types{};

sdk::RETypeDefinition* RETypeDefinition::get_underlying_type() const {
    {
        std::shared_lock _{ g_underlying_mtx };

        if (auto it = g_underlying_types.find(this); it != g_underlying_types.end()) {
            return it->second;
        }
    }
    
    if (!this->is_enum()) {
        return nullptr;
    }

#ifndef RE7
    // get the underlying type of the enum
    // and then hash the name of the type instead
    static auto get_underlying_type_method = this->get_method("GetUnderlyingType");
    const auto underlying_type = get_underlying_type_method->call<::REManagedObject*>(sdk::get_thread_context(), this->get_runtime_type());

    if (underlying_type != nullptr) {
        static auto system_runtime_type_type = sdk::RETypeDB::get()->find_type("System.RuntimeType");
        static auto get_name_method = system_runtime_type_type->get_method("get_FullName");

        const auto full_name = get_name_method->call<::REManagedObject*>(sdk::get_thread_context(), underlying_type);

        if (full_name != nullptr) {
            const auto managed_str = (SystemString*)((uintptr_t)utility::re_managed_object::get_field_ptr(full_name) - sizeof(::REManagedObject));
            const auto str = utility::narrow(managed_str->data);

            managed_str->referenceCount = 0;

            auto type_definition = sdk::RETypeDB::get()->find_type(str);
            
            std::unique_lock _{ g_underlying_mtx };
            g_underlying_types[this] = type_definition;
        }
    }
    
    return g_underlying_types[this];
#else
    const auto value_field = this->get_field("value__");

    if (value_field == nullptr) {
        g_underlying_types[this] = nullptr;
        return nullptr;
    }

    const auto underlying_type = value_field->get_type();
    
    g_underlying_types[this] = underlying_type;
    return g_underlying_types[this];
#endif
}

static std::shared_mutex g_field_mtx{};
static std::unordered_map<std::string, sdk::REField*> g_field_map{};

sdk::REField* RETypeDefinition::get_field(std::string_view name) const {
    auto full_name = std::to_string(this->get_index()) + "." + name.data();

    {
        std::shared_lock _{ g_field_mtx };

        if (auto it = g_field_map.find(full_name); it != g_field_map.end()) {
            return it->second;
        }
    }

    for (auto super = this; super != nullptr; super = super->get_parent_type()) {
        for (auto f : super->get_fields()) {
            if (name == f->get_name()) {
                std::unique_lock _{ g_field_mtx };

                g_field_map[full_name] = f;
                return g_field_map[full_name];
            }
        }
    }

    return g_field_map[full_name];
}

static std::shared_mutex g_method_mtx{};
static std::unordered_map<std::string, REMethodDefinition*> g_method_map{};

sdk::REMethodDefinition* RETypeDefinition::get_method(std::string_view name) const {
    // originally this used this->get_full_name() + "." + name.data()
    // but that doesn't work for generic types if we haven't yet mapped out
    // how generic (instantiated) types work for the game we're working with
    // and this is probably faster anyways
    auto full_name = std::to_string(this->get_index()) + "." + name.data();

    {
        std::shared_lock _{g_method_mtx};

        if (auto it = g_method_map.find(full_name); it != g_method_map.end()) {
            return it->second;
        }
    }
    
    // first pass, do not use function prototypes
    for (auto super = this; super != nullptr; super = super->get_parent_type()) {
        for (auto& m : super->get_methods()) {
            if (name == m.get_name()) {
                std::unique_lock _{g_method_mtx};

                g_method_map[full_name] = &m;
                return g_method_map[full_name];
            }
        }
    }
    
    // second pass, build a function prototype
    for (auto super = this; super != nullptr; super = super->get_parent_type()) {
        for (auto& m : super->get_methods()) {
            const auto method_param_types = m.get_param_types();
            const auto method_param_names = m.get_param_names();

            std::stringstream ss{};
            ss << m.get_name() << "(";

            for (auto i = 0; i < method_param_types.size(); i++) {
                if (i > 0) {
                    ss << ", ";
                }
                ss << method_param_types[i]->get_full_name();
            }

            ss << ")";
            const auto method_prototype = ss.str();

            if (name == method_prototype) {
                std::unique_lock _{g_method_mtx};

                g_method_map[full_name] = &m;
                return g_method_map[full_name];
            }
        }
    }

    return g_method_map[full_name];
}

std::vector<sdk::REMethodDefinition*> RETypeDefinition::get_methods(std::string_view name) const {
    std::vector<sdk::REMethodDefinition*> out{};

    for (auto super = this; super != nullptr; super = super->get_parent_type()) {
        for (auto& m : super->get_methods()) {
            if (name == m.get_name()) {
                out.push_back(&m);
            }
        }
    }

    return out;
}

uint32_t RETypeDefinition::get_index() const {
#ifndef RE7
    return this->index;
#else
    const auto tdb = RETypeDB::get();

    return (uint32_t)(((uintptr_t)this - (uintptr_t)tdb->types) / sizeof(sdk::RETypeDefinition));
#endif
}

int32_t RETypeDefinition::get_fieldptr_offset() const {
#ifndef RE7
    if (this->managed_vt == nullptr) {
        return 0;
    }

    return *(int32_t*)((uintptr_t)this->managed_vt - sizeof(void*));
#else
    auto vm = sdk::VM::get();
    const auto& vm_type = vm->types[this->get_index()];

    return vm_type.fieldptr_offset;
#endif
}

bool RETypeDefinition::has_fieldptr_offset() const {
#ifndef RE7
    return this->managed_vt != nullptr;
#else
    return true;
#endif
}

bool RETypeDefinition::is_a(sdk::RETypeDefinition* other) const {
    if (other == nullptr) {
        return false;
    }

    for (auto super = this; super != nullptr; super = super->get_parent_type()) {
        if (super == other) {
            return true;
        }
    }

    return false;
}

bool RETypeDefinition::is_a(std::string_view other) const {
    return this->is_a(RETypeDB::get()->find_type(other));
}

via::clr::VMObjType RETypeDefinition::get_vm_obj_type() const {
    return (via::clr::VMObjType)this->object_type;
}

void RETypeDefinition::set_vm_obj_type(via::clr::VMObjType type) {
    this->object_type = (uint8_t)type;
}

bool RETypeDefinition::is_value_type() const {
    return get_vm_obj_type() == via::clr::VMObjType::ValType;
}

bool RETypeDefinition::is_enum() const {
    static sdk::RETypeDefinition* enum_type = nullptr;

    if (enum_type == nullptr) {
        enum_type = RETypeDB::get()->find_type("System.Enum");
    }

    if (!this->is_value_type()) {
        return false;
    }

    return this->is_a(enum_type);
}

static std::shared_mutex g_by_ref_mtx{};
static std::unordered_map<const RETypeDefinition*, bool> g_by_ref_map{};

bool RETypeDefinition::is_by_ref() const {
    {
        std::shared_lock _{g_by_ref_mtx};

        if (auto it = g_by_ref_map.find(this); it != g_by_ref_map.end()) {
            return it->second;
        }
    }

    std::unique_lock _{g_by_ref_mtx};

    auto runtime_type = this->get_runtime_type();

    if (runtime_type == nullptr) {
        g_by_ref_map[this] = true;
        return true;
    }

    auto runtime_typedef = utility::re_managed_object::get_type_definition(runtime_type);

    if (runtime_typedef == nullptr) {
        g_by_ref_map[this] = true;
        return true;
    }

    static auto by_ref_method = runtime_typedef->get_method("get_IsByRef");

    g_by_ref_map[this] = by_ref_method->call<bool>(sdk::get_thread_context(), runtime_type);

    return g_by_ref_map[this];   
}

static std::shared_mutex g_pointer_mtx{};
static std::unordered_map<const RETypeDefinition*, bool> g_pointer_map{};

bool RETypeDefinition::is_pointer() const {
    {
        std::shared_lock _{g_pointer_mtx};

        if (auto it = g_pointer_map.find(this); it != g_pointer_map.end()) {
            return it->second;
        }
    }

    std::unique_lock _{g_pointer_mtx};

    auto runtime_type = this->get_runtime_type();

    if (runtime_type == nullptr) {
        g_pointer_map[this] = false;
        return false;
    }

    auto runtime_typedef = utility::re_managed_object::get_type_definition(runtime_type);

    if (runtime_typedef == nullptr) {
        g_pointer_map[this] = false;
        return false;
    }

    static auto pointer_method = runtime_typedef->get_method("get_IsPointer");

    g_pointer_map[this] = pointer_method->call<bool>(sdk::get_thread_context(), runtime_type);

    return g_pointer_map[this];
}

static std::shared_mutex g_primitive_mtx{};
static std::unordered_map<const RETypeDefinition*, bool> g_primitive_map{};

bool RETypeDefinition::is_primitive() const {
    {
        std::shared_lock _{g_primitive_mtx};

        if (auto it = g_primitive_map.find(this); it != g_primitive_map.end()) {
            return it->second;
        }
    }

#ifndef RE7
    std::unique_lock _{g_primitive_mtx};

    auto runtime_type = this->get_runtime_type();

    if (runtime_type == nullptr) {
        g_primitive_map[this] = false;
        return false;
    }

    auto runtime_typedef = utility::re_managed_object::get_type_definition(runtime_type);

    if (runtime_typedef == nullptr) {
        g_primitive_map[this] = false;
        return false;
    }

    static auto primitive_method = runtime_typedef->get_method("get_IsPrimitive");

    g_primitive_map[this] = primitive_method->call<bool>(sdk::get_thread_context(), runtime_type);

    return g_primitive_map[this];
#else
    // RE7 is missing get_IsPrimitive and System.RuntimeType
    const auto full_name_hash = utility::hash(this->get_full_name());

    switch (full_name_hash) {
    case "System.Boolean"_fnv:[[fallthrough]];
    case "System.Char"_fnv:[[fallthrough]];
    case "System.SByte"_fnv:[[fallthrough]];
    case "System.Byte"_fnv:[[fallthrough]];
    case "System.Int16"_fnv:[[fallthrough]];
    case "System.UInt16"_fnv:[[fallthrough]];
    case "System.Int32"_fnv:[[fallthrough]];
    case "System.UInt32"_fnv:[[fallthrough]];
    case "System.Int64"_fnv:[[fallthrough]];
    case "System.UInt64"_fnv:[[fallthrough]];
    case "System.Single"_fnv:[[fallthrough]];
    case "System.Double"_fnv:[[fallthrough]];
    case "System.Void"_fnv:[[fallthrough]];
    case "System.IntPtr"_fnv:[[fallthrough]];
    case "System.UIntPtr"_fnv:
        g_primitive_map[this] = true;
        return true;
    default:
        g_primitive_map[this] = false;
        return false;
    }

    return false;
#endif
}

uint32_t RETypeDefinition::get_crc_hash() const {
#ifndef RE7
    const auto t = get_type();
    return t != nullptr ? t->typeCRC : this->type_crc;
#else
    const auto t = (regenny::via::typeinfo::TypeInfo*)get_type();

    if (t == nullptr) {
        return 0;
    }

    return t->crc;
#endif
}

uint32_t RETypeDefinition::get_fqn_hash() const {
    return this->fqn_hash;
}

uint32_t RETypeDefinition::get_size() const {
#ifndef RE7
    return this->size;
#else
    auto t = (regenny::via::typeinfo::TypeInfo*)get_type();

    if (t == nullptr) {
        return 0;
    }

    return t->size;
#endif
}

uint32_t RETypeDefinition::get_valuetype_size() const {
#if TDB_VER >= 69
    auto tdb = RETypeDB::get();
    auto impl_id = this->impl_index;

    if (impl_id == 0) {
        return 0;
    }

    return (*tdb->typesImpl)[impl_id].field_size;
#else
    return ((REClassInfo*)this)->elementSize;
#endif
}

::REType* RETypeDefinition::get_type() const {
#ifndef RE7
    return this->type;
#else
    auto vm = sdk::VM::get();
    const auto& vm_type = vm->types[this->get_index()];

    return (::REType*)vm_type.reflection_type;
#endif
}

static std::unordered_map<const sdk::RETypeDefinition*, ::REManagedObject*> g_runtime_type_map{};
static std::shared_mutex g_runtime_type_mtx{};

::REManagedObject* RETypeDefinition::get_runtime_type() const {
    {
        std::shared_lock _{g_runtime_type_mtx};

        if (auto it = g_runtime_type_map.find(this); it != g_runtime_type_map.end()) {
            return it->second;
        }
    }

#ifndef RE7
    static auto appdomain_type = sdk::RETypeDB::get()->find_type("System.AppDomain");
    static auto assembly_type = sdk::RETypeDB::get()->find_type("System.Reflection.Assembly");
    static auto get_current_domain_func = appdomain_type->get_method("get_CurrentDomain");
    static auto get_assemblies_func = appdomain_type->get_method("GetAssemblies");
    static auto get_assembly_type_func = assembly_type->get_method("GetType(System.String)");

    auto context = sdk::get_thread_context();
    auto current_domain = get_current_domain_func->call<REManagedObject*>(context, nullptr);

    if (current_domain == nullptr) {
        return nullptr;
    }

    auto assemblies = get_assemblies_func->call<sdk::SystemArray*>(context, current_domain);

    if (assemblies == nullptr) {
        return nullptr;
    }

    const auto assembly_count = assemblies->size();
    const auto managed_string = sdk::VM::create_managed_string(utility::widen(this->get_full_name()));

    for (auto i = 0; i < assembly_count; ++i) {
        auto assembly = (REManagedObject*)assemblies->get_element(i);

        if (assembly == nullptr) {
            continue;
        }

        if (get_assembly_type_func != nullptr) {
            auto type = get_assembly_type_func->call<REManagedObject*>(context, assembly, managed_string);

            if (type != nullptr) {
                std::unique_lock _{g_runtime_type_mtx};
                g_runtime_type_map[this] = type;
                return type;
            }
        } else { // RE7
            static auto get_types_method = assembly_type->get_method("GetTypes");

            if (get_types_method != nullptr) {
                auto types = get_types_method->call<sdk::SystemArray*>(context, assembly);

                if (types != nullptr) {
                    const auto type_count = types->size();

                    for (auto j = 0; j < type_count; ++j) {
                        auto type = (REManagedObject*)types->get_element(j);

                        if (type == nullptr) {
                            continue;
                        }

                        auto type_t = utility::re_managed_object::get_type_definition(type);

                        if (type_t == nullptr) {
                            continue;
                        }

                        static auto get_namespace_method = type_t->get_method("get_Namespace");
                        static auto get_name_method = type_t->get_method("get_Name");

                        auto ns = get_namespace_method->call<SystemString*>(context, type);
                        auto name = get_name_method->call<SystemString*>(context, type);

                        if (ns == nullptr || name == nullptr) {
                            continue;
                        }

                        const auto full_name = utility::narrow(ns->data) + "." + utility::narrow(name->data);

                        if (full_name == this->get_full_name()) {
                            std::unique_lock _{g_runtime_type_mtx};
                            g_runtime_type_map[this] = type;
                            return type;
                        }
                    }
                }
            }
        }
    }
#else
    auto vm = sdk::VM::get();

    if (vm == nullptr) {
        return nullptr;
    }

    const auto& vm_type = vm->types[this->get_index()];

    g_runtime_type_map[this] = (::REManagedObject*)vm_type.runtime_type;
    return g_runtime_type_map[this];
#endif

    return g_runtime_type_map[this];
}

void* RETypeDefinition::get_instance() const {
    const auto t = get_type();

    if (t == nullptr) {
        return nullptr;
    }

    return utility::re_type::get_singleton_instance(t);
}

void* RETypeDefinition::create_instance() const {
    const auto t = get_type();

    if (t == nullptr) {
        return nullptr;
    }

    return utility::re_type::create_instance(t);
}

::REManagedObject* RETypeDefinition::create_instance_full(bool simplify) {
    static auto system_activator_type = sdk::RETypeDB::get()->find_type("System.Activator");
    static auto create_instance_func = system_activator_type->get_method("CreateInstance(System.Type)");
    static auto create_instance_alternative_func = system_activator_type->get_method("createInstance(System.Type)");

    const auto typeof = this->get_runtime_type();

    if (typeof == nullptr) {
        return nullptr;
    }

    if (simplify) {
        // forces the game to use a simplified path for creating the object
        // because in some cases this function could fail
        const auto old_obj_type = this->get_vm_obj_type();
        set_vm_obj_type(via::clr::VMObjType::ValType);

        auto result = create_instance_alternative_func->call<REManagedObject*>(sdk::get_thread_context(), typeof);

        set_vm_obj_type(old_obj_type);

        return result;
    } else {
        return create_instance_func->call<REManagedObject*>(sdk::get_thread_context(), typeof);
    }
}

bool RETypeDefinition::should_pass_by_pointer() const {
    return !is_value_type() || (get_valuetype_size() > sizeof(void*) || (!is_primitive() && !is_enum()));
}
} // namespace sdk