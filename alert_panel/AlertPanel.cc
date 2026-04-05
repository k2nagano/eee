#include "uro_rviz_plugins/AlertPanel.hh"

#include <functional>

#include <QMetaObject>
#include <QVBoxLayout>

#include <pluginlib/class_list_macros.hpp>
#include <rviz_common/display_context.hpp>

using namespace uro_rviz_plugins;

AlertPanel::AlertPanel(QWidget * parent)
: rviz_common::Panel(parent),
  state_label_(new QLabel("SYSTEM ERROR: OFF", this)),
  reset_button_(new QPushButton("Alert Reset", this)),
  overlay_(nullptr),
  alert_active_(false)
{
  auto * layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  state_label_->setAlignment(Qt::AlignCenter);
  state_label_->setMinimumHeight(40);
  state_label_->setStyleSheet(
    "QLabel {"
    "  font-weight: bold;"
    "  color: white;"
    "  background-color: #444444;"
    "  padding: 8px;"
    "  border-radius: 4px;"
    "}");

  reset_button_->setMinimumHeight(36);

  layout->addWidget(state_label_);
  layout->addWidget(reset_button_);
  layout->addStretch(1);

  QObject::connect(reset_button_, &QPushButton::clicked, this, &AlertPanel::onResetClicked);
}

AlertPanel::~AlertPanel()
{
  if (QWidget * top = window()) {
    top->removeEventFilter(this);
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

  if (QWidget * top = window()) {
    top->installEventFilter(this);
  }

  ensureOverlay();
  updateOverlayGeometry();
  setAlertState(false);
}

bool AlertPanel::eventFilter(QObject * watched, QEvent * event)
{
  if (watched == window()) {
    switch (event->type()) {
      case QEvent::Resize:
      case QEvent::Move:
      case QEvent::Show:
      case QEvent::WindowStateChange:
      case QEvent::LayoutRequest:
        updateOverlayGeometry();
        if (overlay_ != nullptr && overlay_->isVisible()) {
          overlay_->raise();
        }
        break;
      default:
        break;
    }
  }

  return rviz_common::Panel::eventFilter(watched, event);
}

void AlertPanel::ensureOverlay()
{
  QWidget * top = window();
  if (top == nullptr) {
    return;
  }

  if (overlay_ != nullptr && overlay_->parentWidget() == top) {
    return;
  }

  if (overlay_ != nullptr) {
    overlay_->deleteLater();
    overlay_ = nullptr;
  }

  overlay_ = new QWidget(top);
  overlay_->setObjectName("uro_alert_overlay");
  overlay_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  overlay_->setAttribute(Qt::WA_StyledBackground, true);
  overlay_->setStyleSheet("#uro_alert_overlay { background-color: rgba(255, 0, 0, 96); }");
  overlay_->hide();
}

void AlertPanel::updateOverlayGeometry()
{
  ensureOverlay();
  if (overlay_ == nullptr) {
    return;
  }

  if (QWidget * top = window()) {
    overlay_->setGeometry(top->rect());
  }
}

void AlertPanel::setAlertState(bool active)
{
  alert_active_ = active;

  if (alert_active_) {
    state_label_->setText("SYSTEM ERROR: ON");
    state_label_->setStyleSheet(
      "QLabel {"
      "  font-weight: bold;"
      "  color: white;"
      "  background-color: #cc0000;"
      "  padding: 8px;"
      "  border-radius: 4px;"
      "}");
    updateOverlayGeometry();
    if (overlay_ != nullptr) {
      overlay_->show();
      overlay_->raise();
    }
  } else {
    state_label_->setText("SYSTEM ERROR: OFF");
    state_label_->setStyleSheet(
      "QLabel {"
      "  font-weight: bold;"
      "  color: white;"
      "  background-color: #444444;"
      "  padding: 8px;"
      "  border-radius: 4px;"
      "}");
    if (overlay_ != nullptr) {
      overlay_->hide();
    }
  }
}

void AlertPanel::systemErrorCallback(const std_msgs::msg::Bool::SharedPtr msg)
{
  const bool active = (msg != nullptr) ? msg->data : false;

  QMetaObject::invokeMethod(
    this,
    [this, active]() {
      setAlertState(active);
    },
    Qt::QueuedConnection);
}

void AlertPanel::onResetClicked()
{
  setAlertState(false);
}

PLUGINLIB_EXPORT_CLASS(uro_rviz_plugins::AlertPanel, rviz_common::Panel)
