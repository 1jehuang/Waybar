#include "modules/waveform.hpp"

#include <cerrno>
#include <csignal>

#include "util/command.hpp"

namespace waybar::modules {
Waveform::Waveform(const std::string& name, const std::string& id, const Json::Value& config,
                   const std::string& output)
    : AModule(config, "waveform-" + name, id) {
  fps_ = waveform::frames_per_second(config["fps"]);
  if (config["width"].isInt()) width_ = std::clamp(config["width"].asInt(), 24, 2048);
  if (config["height"].isInt()) height_ = std::clamp(config["height"].asInt(), 8, 256);
  area_.set_name("waveform-" + name);
  area_.get_style_context()->add_class(MODULE_CLASS);
  if (!id.empty()) area_.get_style_context()->add_class(id);
  event_box_.add(area_);
  area_.signal_draw().connect(sigc::mem_fun(*this, &Waveform::draw));
  area_.show();
  event_box_.set_no_show_all(true);
  event_box_.hide();
  if (!config["exec"].isString()) throw std::runtime_error("waveform requires exec");
  pipe_ = util::command::open(config["exec"].asString(), pid_, output);
  if (!pipe_) throw std::runtime_error("Unable to open waveform exec");
  const int fd = fileno(pipe_);
  if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) == -1) {
    fclose(pipe_);
    pipe_ = nullptr;
    killpg(pid_, SIGTERM);
    std::lock_guard<std::mutex> guard(reap_mtx);
    reap.push_back(pid_);
    throw std::runtime_error("Unable to make waveform transport nonblocking");
  }
  input_ = Glib::signal_io().connect(sigc::mem_fun(*this, &Waveform::read), fd,
                                     Glib::IO_IN | Glib::IO_HUP | Glib::IO_ERR,
                                     Glib::PRIORITY_DEFAULT_IDLE);
}

Waveform::~Waveform() {
  input_.disconnect();
  stopAnimation();
  if (pipe_) fclose(pipe_);
  if (pid_ > 0) {
    killpg(pid_, SIGTERM);
    // Do not block the GTK thread on an uncooperative producer.
    std::lock_guard<std::mutex> guard(reap_mtx);
    reap.push_back(pid_);
  }
}

void Waveform::stopAnimation() {
  if (tick_id_) gtk_widget_remove_tick_callback(GTK_WIDGET(area_.gobj()), tick_id_);
  tick_id_ = 0;
  progress_ = 1;
}

void Waveform::transportError() {
  if (frame_.state == "error" && !frame_.capturing) return;
  frame_.capturing = false;
  frame_.state = "error";
  frame_.tooltip = "Waveform producer stopped or returned invalid data";
  update();
}

bool Waveform::read(Glib::IOCondition condition) {
  // At most 64 KiB per dispatch, and only the newest complete frame survives.
  std::optional<waveform::Frame> latest;
  bool invalid = false, eof = false;
  char buffer[4096];
  for (unsigned budget = 0; budget < 16; ++budget) {
    const auto count = ::read(fileno(pipe_), buffer, sizeof(buffer));
    if (count > 0) {
      lines_.feed(buffer, count, [&](const std::string& line) {
        auto parsed = waveform::parse(line);
        if (parsed) {
          latest = std::move(parsed);
          invalid = false;
        } else
          invalid = true;
      });
    } else {
      if (count == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) eof = true;
      break;
    }
  }
  if (invalid) {
    transportError();
  } else if (latest) {
    const bool changed = (latest->capturing && latest->levels != frame_.levels) ||
                         latest->capturing != frame_.capturing || latest->state != frame_.state ||
                         latest->css_class != frame_.css_class || latest->tooltip != frame_.tooltip;
    previous_ = frame_;
    frame_ = std::move(*latest);
    if (changed) update();
  }
  if (eof || (condition & Glib::IO_ERR)) {
    transportError();
    return false;
  }
  return true;
}

void Waveform::update() {
  auto style = area_.get_style_context();
  if (css_state_ != frame_.state) {
    if (!css_state_.empty()) style->remove_class(css_state_);
    css_state_ = frame_.state;
    if (!css_state_.empty()) style->add_class(css_state_);
  }
  if (css_class_ != frame_.css_class) {
    if (!css_class_.empty()) style->remove_class(css_class_);
    css_class_ = frame_.css_class;
    if (!css_class_.empty()) style->add_class(css_class_);
  }
  if (tooltipEnabled()) event_box_.set_tooltip_text(Glib::ustring(frame_.tooltip).make_valid());
  stopAnimation();
  if (frame_.state == "off") {
    event_box_.hide();
    return;
  }
  area_.set_size_request(frame_.capturing ? width_ : height_, height_);
  event_box_.show();
  const bool changed = frame_.levels != previous_.levels;
  if (frame_.capturing && previous_.capturing && changed && !frame_.levels.empty()) {
    progress_ = 0;
    started_ = g_get_monotonic_time();
    tick_id_ =
        gtk_widget_add_tick_callback(GTK_WIDGET(area_.gobj()), &Waveform::tick, this, nullptr);
  }
  if (!tick_id_) area_.queue_draw();
  AModule::update();
}

gboolean Waveform::tick(GtkWidget*, GdkFrameClock* clock, gpointer data) {
  auto& self = *static_cast<Waveform*>(data);
  const auto now = gdk_frame_clock_get_frame_time(clock);
  if (self.last_tick_ && now - self.last_tick_ < 1000000 / self.fps_) return G_SOURCE_CONTINUE;
  self.last_tick_ = now;
  self.progress_ = std::clamp(
      static_cast<double>(now - self.started_) / (self.frame_.interval_ms * 1000.0), 0.0, 1.0);
  self.area_.queue_draw();
  if (self.progress_ >= 1) {
    self.tick_id_ = 0;
    return G_SOURCE_REMOVE;
  }
  return G_SOURCE_CONTINUE;
}

bool Waveform::draw(const Cairo::RefPtr<Cairo::Context>& cr) {
  const double w = area_.get_allocated_width(), h = area_.get_allocated_height();
  const auto color = area_.get_style_context()->get_color();
  cr->set_source_rgba(color.get_red(), color.get_green(), color.get_blue(), color.get_alpha());
  cairo_set_antialias(cr->cobj(), CAIRO_ANTIALIAS_BEST);
  if (!frame_.capturing) {
    if (frame_.state == "error") {
      cr->set_line_width(2);
      cr->move_to(w / 2, h * .2);
      cr->line_to(w / 2, h * .6);
      cr->stroke();
      cr->arc(w / 2, h * .8, 1, 0, 6.283185307);
      cr->fill();
    } else {
      cr->rectangle(w / 2 - 4, h * .25, 2, h * .5);
      cr->rectangle(w / 2 + 2, h * .25, 2, h * .5);
      cr->fill();
    }
    return true;
  }
  const auto current = waveform::pill_levels(frame_.levels);
  const auto previous = waveform::pill_levels(previous_.levels);
  const double slot = w / current.size();
  const double pill_width = std::clamp(slot * .45, 2.0, 8.0);
  constexpr double pi = 3.141592653589793;
  for (size_t i = 0; i < current.size(); ++i) {
    const double amplitude = current[i] >= previous[i] ? current[i]
        : previous[i] + (current[i] - previous[i]) * progress_;
    const double pill_height = 2 + amplitude * std::max(0.0, h - 6);
    const double radius = std::min(pill_width, pill_height) / 2;
    const double x = (i + .5) * slot - pill_width / 2;
    const double y = (h - pill_height) / 2;
    cr->begin_new_sub_path();
    cr->arc(x + pill_width - radius, y + radius, radius, -pi / 2, 0);
    cr->arc(x + pill_width - radius, y + pill_height - radius, radius, 0, pi / 2);
    cr->arc(x + radius, y + pill_height - radius, radius, pi / 2, pi);
    cr->arc(x + radius, y + radius, radius, pi, 3 * pi / 2);
    cr->close_path();
  }
  cr->fill();
  return true;
}
}  // namespace waybar::modules
