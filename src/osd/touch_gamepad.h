// ██╗  ██╗███████╗██╗███████╗██╗  ██╗ ██████╗
// ██║ ██╔╝██╔════╝██║██╔════╝██║  ██║██╔═══██╗
// █████╔╝ █████╗  ██║███████╗███████║██║   ██║
// ██╔═██╗ ██╔══╝  ██║╚════██║██╔══██║██║   ██║
// ██║  ██╗███████╗██║███████║██║  ██║╚██████╔╝
// ╚═╝  ╚═╝╚══════╝╚═╝╚══════╝╚═╝  ╚═╝ ╚═════╝
//            継 承  ( k e i s h ō )
//
// On-screen multi-touch virtual gamepad and quick menu button.
#pragma once

#include "core/config.h"
#include "core/types.h"
#include "osd/gui.h"

#include <SDL3/SDL.h>

#include <array>
#include <string>
#include <vector>

namespace sm2::osd {

class Input;

class TouchGamepad {
public:
    [[nodiscard]] static bool is_active(const Config& config, const Input& input);

    void set_window(SDL_Window* window) { m_window = window; }

    void reset(Input* input);

    /// Returns true if the touch event landed on a virtual control.
    bool handle_event(const SDL_Event& event, float screen_w, float screen_h,
                      const Config& config, const Gui::GameFamily* family,
                      bool pad_active);

    void update_and_draw(const Config& config, Input* input,
                         const Gui::GameFamily* family,
                         bool in_game, bool pad_active,
                         float screen_w, float screen_h, float ui_scale);

    struct StateAction {
        enum class Kind { None, Save, Load };
        Kind kind = Kind::None;
        int  slot = 1;
    };

    [[nodiscard]] bool take_menu_request();
    [[nodiscard]] bool take_fps_toggle_request();
    [[nodiscard]] bool take_exit_request();
    [[nodiscard]] StateAction take_state_request();
    [[nodiscard]] bool any_touch_claimed() const { return !m_fingers.empty() || m_gun_finger_down; }

    struct ControlOption {
        u32         bit = 0;
        std::string label;
    };

    /// Toggleable controls for `family` (or Default when null).
    [[nodiscard]] static std::vector<ControlOption> describe_controls(
        const Config& config, const Gui::GameFamily* family);

private:
    enum class ControlId {
        None,
        Menu,
        HudFps,
        HudService,
        HudTest,
        HudExit,
        HudView,
        HudSave,
        HudSlot,
        HudLoad,
        Coin,
        Start,
        LeftStick,
        RightStick,
        BtnA,
        BtnB,
        BtnX,
        BtnY,
        BtnLB,
        BtnRB,
        TriggerL,
        TriggerR,
        DpadUp,
        DpadDown,
        DpadLeft,
        DpadRight,
    };

    enum class ActionTarget : u8 {
        None,
        PadRole,
        PadButtonRaw,
        PadAxisPlus,
        AxisRoleAccel,
        AxisRoleBrake,
    };

    struct Widget {
        ControlId    id          = ControlId::None;
        float        cx          = 0.0f;
        float        cy          = 0.0f;
        float        w           = 0.0f;  ///< radius for circles/sticks, width for pills
        float        h           = 0.0f;  ///< height for pills (0 = circle with radius w)
        const char*  label       = "";
        const char*  sublabel    = nullptr;
        bool         circle      = true;
        bool         accent      = false;
        bool         danger      = false;
        ActionTarget target      = ActionTarget::None;
        u8           action_code = 0;
    };

    struct FingerHold {
        SDL_FingerID finger_id = 0;
        ControlId    control   = ControlId::None;
        float        x         = 0.0f;
        float        y         = 0.0f;
    };

    [[nodiscard]] static std::vector<Widget> build_raw_layout(const Config& config,
                                                              const Gui::GameFamily* family,
                                                              bool pad_active,
                                                              float w, float h,
                                                              char* slot_label_buf,
                                                              size_t slot_label_size,
                                                              int active_slot,
                                                              SDL_Window* window = nullptr);

    [[nodiscard]] std::vector<Widget> build_layout(const Config& config,
                                                   const Gui::GameFamily* family,
                                                   bool pad_active,
                                                   float w, float h) const;

    [[nodiscard]] static u32 control_bit(ControlId id);

    [[nodiscard]] static bool hit_test(const Widget& w, float px, float py);

    std::vector<FingerHold> m_fingers;
    bool                    m_gun_finger_down      = false;
    bool                    m_gun_aim_engaged      = false;
    SDL_FingerID            m_gun_aim_finger       = 0;
    float                   m_gun_aim_nx           = 0.5f;
    float                   m_gun_aim_ny           = 0.5f;
    bool                    m_menu_requested       = false;
    bool                    m_fps_toggle_requested = false;
    bool                    m_exit_requested       = false;
    StateAction             m_state_request{};
    int                     m_active_slot          = 1;
    mutable char            m_slot_label[24]       = "SLOT 1";
    SDL_Window*             m_window               = nullptr;
    float                   m_last_w               = 0.0f;
    float                   m_last_h               = 0.0f;
    u32                     m_service_pulse        = 0;
    u32                     m_test_pulse           = 0;
};

}  // namespace sm2::osd
