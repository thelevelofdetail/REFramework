#pragma once

#include <memory>

#include "Mod.hpp"

#include "reframework/API.hpp"

// API manager/proxy to call callbacks added via the C interface
class APIProxy : public Mod {
public:
    static std::shared_ptr<APIProxy>& get();

public:
    std::string_view get_name() const override { return "APIProxy"; }

    void on_frame() override;
    void on_lua_state_created(sol::state& state) override;
    void on_lua_state_destroyed(sol::state& state) override;
    void on_pre_application_entry(void* entry, const char* name, size_t hash) override;
    void on_application_entry(void* entry, const char* name, size_t hash) override;
    void on_device_reset() override;

public:
    using REFInitializedCb = std::function<std::remove_pointer<::REFInitializedCb>::type>;
    using REFLuaStateCreatedCb = std::function<std::remove_pointer<::REFLuaStateCreatedCb>::type>;
    using REFLuaStateDestroyedCb = std::function<std::remove_pointer<::REFLuaStateDestroyedCb>::type>;
    using REFOnFrameCb = std::function<std::remove_pointer<::REFOnFrameCb>::type>;
    using REFOnPreApplicationEntryCb = std::function<std::remove_pointer<::REFOnPreApplicationEntryCb>::type>;
    using REFOnPostApplicationEntryCb = std::function<std::remove_pointer<::REFOnPostApplicationEntryCb>::type>;
    using REFOnDeviceResetCb = std::function<std::remove_pointer<::REFOnDeviceResetCb>::type>;

    static bool add_on_initialized(REFInitializedCb cb); // effectively serves as a do-once callback
    bool add_on_lua_state_created(REFLuaStateCreatedCb cb);
    bool add_on_lua_state_destroyed(REFLuaStateDestroyedCb cb);
    bool add_on_frame(REFOnFrameCb cb);
    bool add_on_pre_application_entry(std::string_view name, REFOnPreApplicationEntryCb cb);
    bool add_on_post_application_entry(std::string_view name, REFOnPostApplicationEntryCb cb);
    bool add_on_device_reset(REFOnDeviceResetCb cb);

private:
    // API Callbacks
    static std::recursive_mutex s_api_cb_mtx;
    static std::vector<APIProxy::REFInitializedCb> s_on_initialized_cbs;
    std::vector<APIProxy::REFLuaStateCreatedCb> m_on_lua_state_created_cbs;
    std::vector<APIProxy::REFLuaStateDestroyedCb> m_on_lua_state_destroyed_cbs;
    std::vector<APIProxy::REFInitializedCb> m_on_frame_cbs{};
    std::vector<APIProxy::REFOnDeviceResetCb> m_on_device_reset_cbs{};

    // Application Entry Callbacks
    std::unordered_map<size_t, std::vector<APIProxy::REFOnPreApplicationEntryCb>> m_on_pre_application_entry_cbs{};
    std::unordered_map<size_t, std::vector<APIProxy::REFOnPostApplicationEntryCb>> m_on_post_application_entry_cbs{};
};