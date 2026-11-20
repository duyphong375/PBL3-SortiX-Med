const { describe, it } = require("node:test");
const assert = require("node:assert");
const fs = require("node:fs");
const path = require("node:path");

describe("Conveyor Running, 3D Visualizer, and Start/Pause Button Synchronization", () => {
  it("1. useConveyorPhysics uses unified isRunning state without OR-bounceback and blocks START during safety locks", () => {
    const physicsPath = path.resolve(__dirname, "../frontend/src/hooks/useConveyorPhysics.ts");
    const content = fs.readFileSync(physicsPath, "utf8");

    assert.doesNotMatch(
      content,
      /isRunningRef\.current\s*\|\|\s*Boolean\(telemetryRef\.current\.conveyor_running\)/,
      "Must not use OR-logic that lets stale telemetry.conveyor_running override Pause"
    );

    assert.match(
      content,
      /const isActualRunning = isRunningRef\.current/,
      "isActualRunning must match synchronized isRunningRef.current"
    );

    assert.match(
      content,
      /lastUserCommandTimeRef:\s*externalLastUserCommandTimeRef/,
      "Must support shared lastUserCommandTimeRef from DashboardLayout"
    );

    assert.match(
      content,
      /telemetryRef\.current\.estop_pressed\s*\|\|\s*isJammedRef\.current\s*\|\|\s*isBinFullRef\.current/,
      "Must block START when E-Stop, Jam, or Bin Full is active"
    );
  });

  it("2. ConveyorVisualizer & ConveyorToolbar sync 3D belt/roller animation, Standby ('Chờ phôi'), and Start/Pause button", () => {
    const visPath = path.resolve(__dirname, "../frontend/src/components/ConveyorVisualizer.tsx");
    const visContent = fs.readFileSync(visPath, "utf8");

    assert.doesNotMatch(
      visContent,
      /isRunning\s*\|\|\s*Boolean\(telemetry\.conveyor_running\)/,
      "ConveyorVisualizer must not use OR-logic for isActualRunning"
    );
    assert.match(visContent, /isActualRunning = isRunning/, "isActualRunning must equal isRunning");
    assert.match(visContent, /isJammed=\{isJammed\}/, "Must pass isJammed to ConveyorToolbar");
    assert.match(visContent, /isBinFull=\{isBinFull\}/, "Must pass isBinFull to ConveyorToolbar");
    assert.match(
      visContent,
      /roller-rotating[\s\S]*?animationDuration/,
      "Both rollers must sync rotation duration with belt speed when moving"
    );

    const toolbarPath = path.resolve(__dirname, "../frontend/src/components/conveyor/ConveyorToolbar.tsx");
    const toolbarContent = fs.readFileSync(toolbarPath, "utf8");

    assert.match(toolbarContent, /"Chờ phôi"/, "Toolbar badge must show 'Chờ phôi' when armed with 0 items");
    assert.match(
      toolbarContent,
      /disabled=\{Boolean\(telemetry\.estop_pressed\s*\|\|\s*isJammed\s*\|\|\s*isBinFull\)\}/,
      "Start/Pause button must be disabled during E-Stop, Jam, or Bin Full"
    );
  });

  it("3. DashboardLayout syncs MQTT & SSE telemetry cleanly with command grace window and simulation hasItems guard", () => {
    const layoutPath = path.resolve(__dirname, "../frontend/src/components/layout/DashboardLayout.tsx");
    const content = fs.readFileSync(layoutPath, "utf8");

    assert.match(
      content,
      /lastUserCommandTimeRef,\s*\n\s*\}\);/,
      "DashboardLayout must share lastUserCommandTimeRef with useConveyorPhysics"
    );
    assert.match(
      content,
      /if\s*\(isRecentUserCmd\)\s*\{\s*updated\.conveyor_running\s*=\s*prev\.conveyor_running;/,
      "Must protect conveyor_running from stale telemetry during user command debounce"
    );
    assert.match(
      content,
      /isSimMode\s*\?\s*isRunningRef\.current\s*&&\s*hasItems\s*&&/,
      "1s simulation heartbeat must require hasItems so conveyor_running does not flicker"
    );
    assert.doesNotMatch(
      content,
      /\(isRunningRef\.current\s*\|\|\s*Boolean\(telemetryRef\.current\.conveyor_running\)\)/,
      "DashboardLayout 1s interval must not use OR-logic in real mode"
    );
  });

  it("4. Overview page and dataProcessor support synchronized conveyor_running, start_btn_pressed, and ldr_active", () => {
    const processorPath = path.resolve(__dirname, "../frontend/src/lib/dataProcessor.ts");
    const processorContent = fs.readFileSync(processorPath, "utf8");

    assert.match(processorContent, /"start_btn_pressed"/, "dataProcessor must preserve start_btn_pressed");
    assert.match(processorContent, /"ldr_active"/, "dataProcessor must preserve ldr_active");

    const overviewPath = path.resolve(__dirname, "../frontend/src/app/page.tsx");
    const overviewContent = fs.readFileSync(overviewPath, "utf8");

    assert.match(overviewContent, /activeVisualItemsCount/, "Overview KPI must count only active visual items");
    assert.match(
      overviewContent,
      /isStartBtnActive\s*=\s*isSimulation\s*\?\s*isRunning\s*:\s*Boolean\(telemetry\.start_btn_pressed\)/,
      "Overview start button status must reflect simulation state or hardware GPIO23"
    );
  });

  it("5. Firmware MQTT and HTTP telemetry publish start_btn_pressed and ldr_active", () => {
    const mqttC = path.resolve(__dirname, "../../sortix-firmware/components/mqtt_handler/mqtt_handler.c");
    const httpC = path.resolve(__dirname, "../../sortix-firmware/components/http_telemetry/http_telemetry.c");

    assert.match(fs.readFileSync(mqttC, "utf8"), /"start_btn_pressed"/, "MQTT telemetry must include start_btn_pressed");
    assert.match(fs.readFileSync(mqttC, "utf8"), /"ldr_active"/, "MQTT telemetry must include ldr_active");
    assert.match(fs.readFileSync(httpC, "utf8"), /"start_btn_pressed"/, "HTTP telemetry must include start_btn_pressed");
    assert.match(fs.readFileSync(httpC, "utf8"), /"ldr_active"/, "HTTP telemetry must include ldr_active");
  });

  it("6. TelemetrySchema accepts signed negative encoder_count and useMQTT ignores ai_server status & merges buffered telemetry", () => {
    const schemaPath = path.resolve(__dirname, "../shared/schemas/index.ts");
    const schemaContent = fs.readFileSync(schemaPath, "utf8");
    assert.doesNotMatch(
      schemaContent,
      /encoder_count:\s*finiteNumber\.nonnegative\(\)/,
      "TelemetrySchema must accept signed negative encoder_count from bidirectional PCNT hardware"
    );

    const mqttHookPath = path.resolve(__dirname, "../frontend/src/hooks/useMQTT.ts");
    const mqttContent = fs.readFileSync(mqttHookPath, "utf8");
    assert.match(
      mqttContent,
      /data\.source\s*!==\s*"ai_server"/,
      "useMQTT must ignore ai_server status messages in telemetry coalescing"
    );
  });
});
