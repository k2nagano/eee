#include "uro_rviz_plugins/AlertPanel.hh"

#include <functional>

#include <QApplication>
#include <QMainWindow>
#include <QMetaObject>
#include <QVBoxLayout>

#include <pluginlib/class_list_macros.hpp>
#include <rviz_common/display_context.hpp>

using namespace uro_rviz_plugins;

namespace
{

constexpr int kOverlayLabelMargin = 10;
constexpr int kOverlayLabelWidth = 360;
constexpr int kOverlayLabelHeight = 44;

bool isGeometryRelatedEvent(QEvent::Type type)
{
  switch (type) {
    case QEvent::Resize:
    case QEvent::Move:
    case QEvent::Show:
    case QEvent::WindowStateChange:
    case QEvent::LayoutRequest:
    case QEvent::ZOrderChange:
      return true;
    default:
      return false;
  }
}

}  // namespace

AlertPanel::AlertPanel(QWidget * parent)
: rviz_common::Panel(parent),
  reset_button_(new QPushButton("Alert Reset", this)),
  overlay_(nullptr),
  overlay_label_button_(nullptr),
  alert_active_(false)
{
  auto * layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  reset_button_->setMinimumHeight(36);
  layout->addWidget(reset_button_);
  layout->addStretch(1);

  QObject::connect(reset_button_, &QPushButton::clicked, this, &AlertPanel::onResetClicked);
}

AlertPanel::~AlertPanel()
{
  if (main_window_ != nullptr) {
    main_window_->removeEventFilter(this);
  }

  if (overlay_label_button_ != nullptr) {
    overlay_label_button_->hide();
    overlay_label_button_->deleteLater();
    overlay_label_button_ = nullptr;
  }

  if (overlay_ != nullptr) {
    overlay_->hide();
    overlay_->deleteLater();
    overlay_ = nullptr;
  }
}

void AlertPanel::onInitialize()
{
  rviz_common::Panel::onInitialize();

  node_ptr_ = getDisplayContext()->getRosNodeAbstraction().lock();
  if (!node_ptr_) {
    RCLCPP_ERROR(rclcpp::get_logger("AlertPanel"), "Failed to get RViz ROS node abstraction.");
    return;
  }

  auto raw_node = node_ptr_->get_raw_node();
  subscription_ = raw_node->create_subscription<std_msgs::msg::Bool>(
    "/system_error",
    rclcpp::QoS(1),
    std::bind(&AlertPanel::systemErrorCallback, this, std::placeholders::_1));

  ensureOverlayWidgets();
  ensureMainWindowEventFilter();
  updateOverlayGeometry();
  setAlertState(false);
}

QWidget * AlertPanel::findRvizMainWindow() const
{
  QWidget * best = nullptr;
  int best_area = -1;

  const auto top_levels = QApplication::topLevelWidgets();
  for (QWidget * widget : top_levels) {
    if (widget == nullptr || !widget->isVisible()) {
      continue;
    }

    auto * main_window = qobject_cast<QMainWindow *>(widget);
    if (main_window == nullptr) {
      continue;
    }

    const QRect g = main_window->geometry();
    const int area = g.width() * g.height();
    if (area > best_area) {
      best = main_window;
      best_area = area;
    }
  }

  if (best != nullptr) {
    return best;
  }

  QWidget * panel_window = window();
  auto * as_main_window = qobject_cast<QMainWindow *>(panel_window);
  if (as_main_window != nullptr) {
    return as_main_window;
  }

  return panel_window;
}

void AlertPanel::ensureMainWindowEventFilter()
{
  QWidget * current_main_window = findRvizMainWindow();
  if (current_main_window == nullptr) {
    return;
  }

  if (main_window_ == current_main_window) {
    return;
  }

  if (main_window_ != nullptr) {
    main_window_->removeEventFilter(this);
  }

  main_window_ = current_main_window;
  main_window_->installEventFilter(this);
}

bool AlertPanel::eventFilter(QObject * watched, QEvent * event)
{
  ensureMainWindowEventFilter();

  if (watched == main_window_ && event != nullptr && isGeometryRelatedEvent(event->type())) {
    updateOverlayGeometry();
    if (alert_active_) {
      raiseAlertWidgets();
    }
  }

  return rviz_common::Panel::eventFilter(watched, event);
}

void AlertPanel::ensureOverlayWidgets()
{
  ensureMainWindowEventFilter();
  QWidget * target = main_window_ != nullptr ? main_window_.data() : findRvizMainWindow();
  if (target == nullptr) {
    return;
  }

  if (overlay_ != nullptr && overlay_->parentWidget() != target) {
    overlay_->deleteLater();
    overlay_ = nullptr;
  }

  if (overlay_label_button_ != nullptr && overlay_label_button_->parentWidget() != target) {
    overlay_label_button_->deleteLater();
    overlay_label_button_ = nullptr;
  }

  if (overlay_ == nullptr) {
    overlay_ = new QWidget(target);
    overlay_->setObjectName("uro_alert_overlay");
    overlay_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    overlay_->setAttribute(Qt::WA_StyledBackground, true);
    overlay_->setStyleSheet("#uro_alert_overlay { background-color: rgba(255, 0, 0, 96); }");
    overlay_->hide();
  }

  if (overlay_label_button_ == nullptr) {
    overlay_label_button_ = new QPushButton(target);
    overlay_label_button_->setObjectName("uro_alert_overlay_label");
    overlay_label_button_->setText("SYSTEM ERROR - CLICK HERE TO RESET");
    overlay_label_button_->setCursor(Qt::PointingHandCursor);
    overlay_label_button_->setFlat(false);
    overlay_label_button_->setStyleSheet(
      "QPushButton#uro_alert_overlay_label {"
      "  background-color: #cc0000;"
      "  color: yellow;"
      "  font-weight: bold;"
      "  text-align: left;"
      "  padding-left: 10px;"
      "  border: 2px solid yellow;"
      "}"
      "QPushButton#uro_alert_overlay_label:pressed {"
      "  background-color: #aa0000;"
      "}");
    overlay_label_button_->hide();

    QObject::connect(
      overlay_label_button_, &QPushButton::clicked, this, &AlertPanel::onOverlayLabelClicked);
  }
}

void AlertPanel::updateOverlayGeometry()
{
  ensureOverlayWidgets();

  QWidget * target = main_window_ != nullptr ? main_window_.data() : findRvizMainWindow();
  if (target == nullptr) {
    return;
  }

  if (overlay_ != nullptr) {
    overlay_->setGeometry(target->rect());
  }

  if (overlay_label_button_ != nullptr) {
    overlay_label_button_->setGeometry(
      kOverlayLabelMargin,
      kOverlayLabelMargin,
      kOverlayLabelWidth,
      kOverlayLabelHeight);
  }
}

void AlertPanel::raiseAlertWidgets()
{
  if (overlay_ != nullptr) {
    overlay_->raise();
  }
  if (overlay_label_button_ != nullptr) {
    overlay_label_button_->raise();
  }
}

void AlertPanel::setAlertState(bool active)
{
  alert_active_ = active;

  ensureOverlayWidgets();
  updateOverlayGeometry();

  if (alert_active_) {
    if (overlay_ != nullptr) {
      overlay_->show();
    }
    if (overlay_label_button_ != nullptr) {
      overlay_label_button_->show();
    }
    raiseAlertWidgets();
  } else {
    if (overlay_label_button_ != nullptr) {
      overlay_label_button_->hide();
    }
    if (overlay_ != nullptr) {
      overlay_->hide();
    }
  }
}

void AlertPanel::systemErrorCallback(const std_msgs::msg::Bool::SharedPtr msg)
{
  const bool active = (msg != nullptr) ? msg->data : false;

  if (!active) {
    return;
  }

  QMetaObject::invokeMethod(
    this,
    [this]() {
      setAlertState(true);
    },
    Qt::QueuedConnection);
}

void AlertPanel::onResetClicked()
{
  setAlertState(false);
}

void AlertPanel::onOverlayLabelClicked()
{
  setAlertState(false);
}

PLUGINLIB_EXPORT_CLASS(uro_rviz_plugins::AlertPanel, rviz_common::Panel)
