# Communication Protocol

This protocol governs all MQTT‑based messaging between an Arduino Mega (“Arduino”) and a host Computer (“Computer”).  
**Message format**: JSON  
**Topics**:  
- From Arduino → Computer: `arduino/out/<component>/<action>`  
- From Computer → Arduino: `computer/out/<component>/<action>`

---

## 1. Heartbeat / Checkup

| Direction | Topic                     | Payload      | Expected Response                        | Timeout Behavior                                       |
|-----------|---------------------------|--------------|------------------------------------------|--------------------------------------------------------|
| Arduino → Computer | `arduino/out/checkup`       | `{}`         | Computer must publish `computer/out/checkup` within 1 s. | If Arduino sees 3 missed responses → **Emergency Stop**. |
| Computer → Arduino | `computer/out/checkup`       | `{}`         | —                                        | If Computer sees no checkup in 3 s → **Pause Processes**. |

---

## 2. Rotation (Joint Angles)

> **Payload schema**  
> ```json
> { "0": <float>, "1": <float>, ..., "5": <float> }
> ```
> Keys `0–5` correspond to joints 0–5.

| Direction | Topic                            | Payload                           | Expected Response            |
|-----------|----------------------------------|-----------------------------------|------------------------------|
| Arduino → Computer | `arduino/out/rotation/current`   | rotation payload                  | —                            |
| Arduino → Computer | `arduino/out/rotation/velocity`  | rotation payload                  | —                            |
| Arduino → Computer | `arduino/out/rotation/target`    | rotation payload                  | —                            |
| Arduino → Computer | `arduino/out/rotation/complete`    | `{}`                  | —                            |
| Computer → Arduino | `computer/out/rotation/info`     | `{}`                     | Arduino must publish its current, velocity & target on the three topics above. |
| Computer → Arduino | `computer/out/rotation/target`   | rotation payload                  | Arduino must, upon reaching target, publish `arduino/out/rotation/complete`. |

---

## 3. Gripper

> **Payload schema**  
> ```json
> { "open": <bool> }
> ```

| Direction | Topic                       | Payload                   | Expected Response                         |
|-----------|-----------------------------|---------------------------|-------------------------------------------|
| Arduino → Computer | `arduino/out/graper/state`   | gripper payload           | —                                         |
| Arduino → Computer | `arduino/out/graper/complete`   | `{}`           | —                                         |
| Computer → Arduino | `computer/out/graper/info`    | `{}`                       | Arduino must publish its current state.   |
| Computer → Arduino | `computer/out/graper/target`  | gripper payload           | Upon action completion, Arduino → `arduino/out/action/complete`. |

---

## 4. Health Monitoring

> **Payload schema**  
> ```json
> { "ok": <bool>, "message": <string> }
> ```
> `ok=false` indicates an error; `message` describes the issue.

| Direction | Topic                       | Payload                   | Expected Response                                 |
|-----------|-----------------------------|---------------------------|---------------------------------------------------|
| Arduino → Computer | `arduino/out/health/status` | health payload            | —                                                 |
| Computer → Arduino | `computer/out/health/info`   | `{}`                       | Arduino must publish its health status afterward. |

---

## Notes & Naming Conventions

1. **Topic hierarchy** follows `<device>/out/<component>/<action>`.  
2. Use **empty** `{}` JSON when no data is needed.  
3. Always mirror “info” topics with their corresponding data topics.  
4. **`action/complete`** is a catch‑all topic:  
   ```txt
   Topic: arduino/out/action/complete
   Payload: {}
   ```
   Use this after any commanded target (rotation or gripper) is reached.  
