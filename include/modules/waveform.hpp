#pragma once

#include "AModule.hpp"
#include "modules/waveform_data.hpp"

namespace waybar::modules {
class Waveform : public AModule {
 public:
  Waveform(const std::string& name, const std::string& id, const Json::Value& config,
           const std::string& output);
  ~Waveform() override;
  void update() override;

 private:
  bool read(Glib::IOCondition condition);
  bool draw(const Cairo::RefPtr<Cairo::Context>& cr);
  static gboolean tick(GtkWidget*, GdkFrameClock*, gpointer);
  void stopAnimation();
  void transportError();
  Gtk::DrawingArea area_;
  FILE* pipe_ = nullptr;
  int pid_ = -1;
  sigc::connection input_;
  waveform::Lines lines_;
  waveform::Frame frame_, previous_;
  std::string css_state_, css_class_;
  guint tick_id_ = 0;
  gint64 started_ = 0, last_tick_ = 0;
  double progress_ = 1;
  int width_ = 180, height_ = 24;
};
}  // namespace waybar::modules
