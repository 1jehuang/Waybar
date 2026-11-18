#include <fcntl.h>
#include <gtkmm.h>
#include <unistd.h>

#include <cstdint>
#include <iostream>
#include <list>
#include <mutex>

#include "modules/waveform.hpp"

std::mutex reap_mtx;
std::list<pid_t> reap;

class TestWaveform : public waybar::modules::Waveform {
 public:
  using Waveform::Waveform;
  size_t clicks() const { return pid_children_.size(); }
};

static void pump(unsigned ms) {
  const auto until = g_get_monotonic_time() + ms * 1000;
  do {
    while (Gtk::Main::events_pending()) Gtk::Main::iteration(false);
    g_usleep(1000);
  } while (g_get_monotonic_time() < until);
}

int main(int argc, char** argv) {
  Gtk::Main main(argc, argv);
  int source[2];
  if (pipe(source)) return 1;
  fcntl(source[1], F_SETFD, FD_CLOEXEC);
  Json::Value config;
  config["exec"] = "cat <&" + std::to_string(source[0]);
  config["on-click"] = "true";
  config["fps"] = 120;  // The native renderer must cap this at 60.
  TestWaveform module("jarvis", "", config, "test-output");
  auto& box = dynamic_cast<Gtk::EventBox&>(static_cast<Gtk::Widget&>(module));
  auto* area = dynamic_cast<Gtk::DrawingArea*>(box.get_child());
  if (!area) return 2;
  auto css = Gtk::CssProvider::create();
  css->load_from_data("#waveform-jarvis { color: white; background-color: transparent; }");
  area->get_style_context()->add_provider(css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  Gtk::Window window;
  window.add(box);
  window.show();
  unsigned draws = 0;
  area->signal_draw().connect(
      [&](const Cairo::RefPtr<Cairo::Context>&) {
        ++draws;
        return false;
      },
      false);
  auto send = [&](const std::string& text) {
    const auto record = text + "\n";
    if (::write(source[1], record.data(), record.size()) != static_cast<ssize_t>(record.size()))
      throw std::runtime_error("write failed");
    pump(120);
  };
  send(R"({"levels":[],"capturing":false,"state":"off"})");
  if (box.get_visible()) return 3;
  send(R"({"levels":[0,0.5,1,0],"capturing":true,"state":"listening","sequence":1})");
  if (!box.get_visible() || !area->get_style_context()->has_class("listening") || draws == 0)
    return 4;
  const auto before_animation = draws;
  send(R"({"levels":[0.5,1,0,0.7],"capturing":true,"state":"listening","sequence":2})");
  if (draws <= before_animation + 1) return 11;
  auto settled = draws;
  pump(160);
  if (draws != settled) return 5;
  auto hasQuietPills = [&] {
    const int w = area->get_allocated_width(), h = area->get_allocated_height();
    auto image = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, w, h);
    auto context = Cairo::Context::create(image);
    gtk_widget_draw(GTK_WIDGET(area->gobj()), context->cobj());
    image->flush();
    auto alpha = [&](int x, int y) {
      const auto* row =
          reinterpret_cast<const std::uint32_t*>(image->get_data() + y * image->get_stride());
      return row[x] >> 24;
    };
    unsigned runs = 0;
    bool inside = false;
    for (int x = 0; x < w; ++x) {
      const bool visible = alpha(x, h / 2) > 0;
      if (visible && !inside) ++runs;
      inside = visible;
    }
    if (runs != 7) return false;
    for (unsigned pill = 0; pill < 7; ++pill) {
      const int x = static_cast<int>((pill + .5) * w / 7);
      unsigned rows = 0;
      for (int y = 0; y < h; ++y)
        if (alpha(x, y) > 0) ++rows;
      if (rows != 2) return false;
    }
    return true;
  };
  send(R"({"levels":[0,0,0],"capturing":true,"state":"listening","sequence":3})");
  if (!hasQuietPills()) return 14;
  send(R"({"levels":[],"capturing":true,"state":"listening","sequence":4})");
  if (!hasQuietPills()) return 15;
  send(R"({"levels":[],"capturing":false,"state":"paused"})");
  int width, height;
  area->get_size_request(width, height);
  if (!box.get_visible() || width != height) return 6;
  settled = draws;
  send(R"({"levels":[],"capturing":false,"state":"paused"})");
  if (draws != settled) return 7;
  send(R"({"levels":[1,0.5,1],"capturing":false,"state":"paused"})");
  if (draws != settled) return 12;
  GdkEventButton event{};
  event.type = GDK_BUTTON_PRESS;
  event.button = 1;
  gboolean handled = false;
  g_signal_emit_by_name(box.gobj(), "button-press-event", &event, &handled);
  if (!handled || module.clicks() != 1) return 8;
  send("invalid JSON");
  if (!area->get_style_context()->has_class("error")) return 9;
  send(R"({"levels":[],"capturing":false,"state":"off"})");
  if (box.get_visible()) return 10;
  close(source[1]);
  pump(120);
  if (!box.get_visible() || !area->get_style_context()->has_class("error")) return 13;
  window.remove();
  close(source[0]);
  std::cout
      << "Waveform GTK transport, finite animation, idle pause, recovery and click checks passed\n";
}
