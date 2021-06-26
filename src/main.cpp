#include <chrono>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <termox/termox.hpp>

#include <cli.hpp>
#include <gameboy.hpp>
#include <input.hpp>
#include <options.hpp>
#include <util/files.hpp>

namespace {

constexpr auto gb_width  = 160;
constexpr auto gb_height = 144;

auto load_state(std::string const& save_name) -> std::vector<u8>
{
    auto const filename = save_name + ".sav";
    auto const ifs      = std::ifstream{filename};
    if (ifs.good()) {
        auto save_data = read_bytes(filename);
        return save_data;
    }
    else
        return {};
}

}  // namespace

namespace gbterm {

class Gameboy_widget
    : public ox::
          Color_graph_static_bounds<int, 0, gb_width - 1, gb_height - 1, 0> {
   private:
    static constexpr auto display_width  = gb_width - 1;
    static constexpr auto display_height = (gb_height - 1) / 2;

    using Base_t =
        Color_graph_static_bounds<int, 0, gb_width - 1, gb_height - 1, 0>;

    using Clock_t = std::chrono::high_resolution_clock;

   public:
    Gameboy_widget(Cartridge& cart, Options& options) : emulator_{cart, options}
    {
        using namespace ox::pipe;

        ox::Terminal::set_palette(ox::gameboy::palette);
        *this | maximum_width(display_width) | maximum_height(display_height) |
            strong_focus();

        emulator_.register_draw_callback([this, previous_time = Clock_t::now()](
                                             FrameBuffer const& buf) mutable {
            constexpr auto zero   = Clock_t::duration{0};
            constexpr auto period = std::chrono::microseconds{16'667};  // 60fps
            auto const to_wait    = period - (Clock_t::now() - previous_time);
            if (to_wait > zero)
                std::this_thread::sleep_for(to_wait);
            previous_time = Clock_t::now();
            next_buffer_  = buf;
        });

        loop_.run_async([this](auto& queue) {
            while (!next_buffer_.has_value())
                emulator_.tick();  // This can assign to next_buffer_.

            queue.append(
                ox::Custom_event{[this, buf = std::move(*next_buffer_)] {
                    this->handle_next_frame(std::move(buf));
                }});
            next_buffer_ = std::nullopt;
        });
    }

   protected:
    auto key_press_event(ox::Key k) -> bool override
    {
        if (auto const button = key_to_button(k); button.has_value())
            emulator_.button_pressed(*button);
        return Base_t::key_press_event(k);
    }

    auto key_release_event(ox::Key k) -> bool override
    {
        if (auto const button = key_to_button(k); button.has_value())
            emulator_.button_released(*button);
        return Base_t::key_release_event(k);
    }

   private:
    Gameboy emulator_;
    ox::Event_loop loop_;
    std::optional<::FrameBuffer> next_buffer_ = std::nullopt;

    std::vector<std::pair<Base_t::Coordinate, ox::Color>> optimization_buf_;

   private:
    void handle_next_frame(FrameBuffer buf)
    {
        auto const& coords = translate_to_pairs(buf);
        this->Base_t::reset(std::cbegin(coords), std::cend(coords));
    }

    /// Translate ox::Key to gameboy button, if there is a representation.
    [[nodiscard]] static auto key_to_button(ox::Key k)
        -> std::optional<::GbButton>
    {
        using ox::Key;
        switch (k) {
            case Key::Arrow_up: return ::GbButton::Up;
            case Key::Arrow_down: return ::GbButton::Down;
            case Key::Arrow_left: return ::GbButton::Left;
            case Key::Arrow_right: return ::GbButton::Right;
            case Key::z: return ::GbButton::A;
            case Key::x: return ::GbButton::B;
            case Key::Enter: return ::GbButton::Start;
            case Key::Backspace: return ::GbButton::Select;
            default: return std::nullopt;
        }
    }

    [[nodiscard]] auto translate_to_pairs(FrameBuffer const& buf)
        -> std::vector<std::pair<Base_t::Coordinate, ox::Color>>&
    {
        optimization_buf_.clear();
        for (auto x = 0; x < gb_width; ++x) {
            for (auto y = 0; y < gb_height; ++y) {
                optimization_buf_.push_back({{x, Base_t::boundary().north - y},
                                             to_color(buf.get_pixel(x, y))});
            }
        }
        return optimization_buf_;
    }

    [[nodiscard]] static auto to_color(::Color c) -> ox::Color
    {
        switch (c) {
            case ::Color::White: return ox::gameboy::Green_4;
            case ::Color::LightGray: return ox::gameboy::Green_3;
            case ::Color::DarkGray: return ox::gameboy::Green_2;
            case ::Color::Black: return ox::gameboy::Green_1;
        }
    }
};

struct App : ox::Float_2d<Gameboy_widget> {
    App(Cartridge& cart, Options& options)
        : ox::Float_2d<Gameboy_widget>{cart, options}
    {
        using namespace ox::pipe;
        *this | direct_focus() | forward_focus(this->widget.widget);
    }
};

}  // namespace gbterm

int main(int argc, char* argv[])
{
    auto cli_options = get_cli_options(argc, argv);
    auto cartridge   = get_cartridge(read_bytes(cli_options.filename),
                                   load_state(cli_options.filename));

    return ox::System{ox::Mouse_mode::Basic, ox::Key_mode::Raw}
        .run<gbterm::App>(*cartridge, cli_options.options);
}
