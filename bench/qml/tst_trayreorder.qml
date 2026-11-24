import QtQuick
import QtTest
import "../../plasma/tray/qml" as Tray

Item {
    id: scene
    width: 200
    height: 100

    Flickable {
        id: viewport
        anchors.fill: parent
        contentWidth: 400
        contentHeight: height
        boundsBehavior: Flickable.StopAtBounds
    }

    Rectangle {
        id: network
        parent: viewport.contentItem
        property string itemId: "network"
        x: 16
        y: 32
        width: 16
        height: 16
        color: "blue"
    }
    Rectangle {
        id: volume
        parent: viewport.contentItem
        property string itemId: "volume"
        x: 56
        y: 32
        width: 16
        height: 16
        color: "green"
    }
    Tray.IconReorderArea {
        id: area
        parent: viewport.contentItem
        x: 8
        y: 24
        width: 80
        height: 32
        items: [network, volume]
        order: ["network", "hidden-battery", "volume"]
        indicatorColor: "red"
        onOrderRequested: order => area.order = order
    }
    SignalSpy { id: clickSpy; target: area; signalName: "clicked" }
    SignalSpy { id: pressSpy; target: area; signalName: "pressed" }
    SignalSpy { id: wheelSpy; target: area; signalName: "wheel" }
    SignalSpy { id: orderSpy; target: area; signalName: "orderRequested" }

    TestCase {
        name: "TrayReorder"
        when: windowShown

        function init() {
            viewport.cancelFlick();
            viewport.contentX = 0;
            area.order = ["network", "hidden-battery", "volume"];
            clickSpy.clear();
            pressSpy.clear();
            wheelSpy.clear();
            orderSpy.clear();
        }

        function test_clicks_data() {
            return [{ tag: "left", button: Qt.LeftButton },
                    { tag: "middle", button: Qt.MiddleButton },
                    { tag: "right", button: Qt.RightButton }];
        }

        function test_clicks(data) {
            mouseClick(area, 16, 16, data.button);
            compare(clickSpy.count, 1);
            compare(pressSpy.count, 1);
            compare(area.draggedId, "network");
            compare(area.dragging, false);
            compare(orderSpy.count, 0);
        }

        function test_wheel() {
            mouseWheel(area, 56, 16, 0, 120);
            compare(wheelSpy.count, 1);
            compare(orderSpy.count, 0);
        }

        function test_smallMovementClicks() {
            mousePress(area, 16, 16);
            mouseMove(area, 17, 16);
            mouseRelease(area, 17, 16);
            compare(clickSpy.count, 1);
            compare(area.dragging, false);
        }

        function test_dragSuppressesClick() {
            mousePress(area, 16, 16);
            mouseMove(area, 64, 16);
            wait(0);
            mouseRelease(area, 64, 16);
            tryCompare(area, "dragging", false);
            compare(clickSpy.count, 0);
            compare(area.draggedId, "network");
            compare(area.order, ["hidden-battery", "volume", "network"]);
        }

        function test_orderPreservesHiddenItems() {
            area.moveBefore("volume", "network");
            compare(area.order, ["volume", "network", "hidden-battery"]);
            area.moveBefore("volume", "");
            compare(area.order, ["network", "hidden-battery", "volume"]);
            orderSpy.clear();
            area.moveBefore("volume", "volume");
            area.moveBefore("missing", "network");
            compare(orderSpy.count, 0);
        }

        function test_dragBeforeKeepsParentStill() {
            mousePress(area, 56, 16);
            mouseMove(area, 36, 16, 20);
            mouseMove(area, 8, 16, 20);
            compare(area.dragging, true);
            compare(area.insertAt, 0);
            mouseRelease(area, 8, 16);
            compare(orderSpy.count, 1);
            compare(area.order, ["volume", "network", "hidden-battery"]);
            compare(viewport.contentX, 0);
            compare(area.x, 8);
            compare(area.y, 24);
            compare(area.dragging, false);
        }

        function test_dragAfter() {
            mouseDrag(area, 16, 16, 63, 0);
            compare(area.order, ["hidden-battery", "volume", "network"]);
            compare(area.dragging, false);
            compare(clickSpy.count, 0);
        }

        function test_dragOutsideCancels() {
            mousePress(area, 16, 16);
            mouseMove(area, 120, 16);
            compare(area.dragging, true);
            compare(area.insertAt, -1);
            mouseRelease(area, 120, 16);
            compare(area.dragging, false);
            compare(orderSpy.count, 0);
            compare(clickSpy.count, 0);
        }

        function test_dragBackInside() {
            mousePress(area, 16, 16);
            mouseMove(area, 120, 16);
            mouseMove(area, 79, 16);
            compare(area.dragging, true);
            mouseRelease(area, 79, 16);
            compare(area.order, ["hidden-battery", "volume", "network"]);
            compare(area.dragging, false);
        }

        function test_rightButtonDoesNotReorder() {
            mouseDrag(area, 56, 16, -48, 0, Qt.RightButton);
            compare(area.dragging, false);
            compare(orderSpy.count, 0);
        }

        function benchmark_hitTesting() {
            area.itemAt(48);
            area.indexAt(48);
        }
    }
}
