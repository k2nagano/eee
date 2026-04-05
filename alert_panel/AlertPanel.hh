#ifndef URO_RVIZ_PLUGINS__ALERT_PANEL_HH_
#define URO_RVIZ_PLUGINS__ALERT_PANEL_HH_

#include <memory>

#include <QEvent>
#include <QPointer>
#include <QPushButton>
#include <QWidget>

#include <rclcpp/rclcpp.hpp>
#include <rviz_common/panel.hpp>
#include <rviz_common/ros_integration/ros_node_abstraction_iface.hpp>
#include <std_msgs/msg/bool.hpp>

namespace uro_rviz_plugins
{

class AlertPanel : public rviz_common::Panel
{
  Q_OBJECT

public:
  explicit AlertPanel(QWidget * parent = nullptr);
  ~AlertPanel() override;

  void onInitialize() override;

protected:
  bool eventFilter(QObject * watched, QEvent * event) override;

private Q_SLOTS:
  void onResetClicked();
  void onOverlayLabelClicked();

private:
  QWidget * findRvizMainWindow() const;
  void ensureOverlayWidgets();
  void ensureMainWindowEventFilter();
  void updateOverlayGeometry();
  void raiseAlertWidgets();
  void setAlertState(bool active);
  void systemErrorCallback(const std_msgs::msg::Bool::SharedPtr msg);

  std::shared_ptr<rviz_common::ros_integration::RosNodeAbstractionIface> node_ptr_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr subscription_;

  QPushButton * reset_button_;
  QPointer<QWidget> main_window_;
  QWidget * overlay_;
  QPushButton * overlay_label_button_;
  bool alert_active_;
};

}  // namespace uro_rviz_plugins

#endif  // URO_RVIZ_PLUGINS__ALERT_PANEL_HH_
