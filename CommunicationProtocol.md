# Wifi Communication Protocol (Arduino Mega ↔ Computer)

This protocol governs all MQTT‑based messaging between an Arduino Mega (“Arduino”) and a host Computer (“Computer”).  
**Message format**: JSON  
**Topics**:  
- From Arduino → Computer: `arduino/out/<component>/<action>` 
- From Arduino → Computer: `arduino/confirm/<component>/<action>` *Recieve conformation message*
- From Computer → Arduino: `computer/out/<component>/<action>`

## 1. Messages
### 1. Heartbeat / Checkup

| Direction | Topic                     | Payload      | Expected Response                        | Timeout Behavior                                       |
|-|-|-|-|-|
| Arduino → Computer | `arduino/out/checkup` | checkup payload | Computer must publish `computer/out/checkup` within 1 s. | If Arduino sees 3 missed responses → **Emergency Stop**. |
| Computer → Arduino | `computer/out/checkup` | `{}` | - | If Computer sees no checkup in 3 s → **Pause Processes**. |

### 2. Rotation (Joint Angles)

| Direction | Topic | Payload | Expected Response |
|-|-|-|-|
| **On Event**
| Arduino → Computer | `arduino/out/rotation/target/reached` | `{"buffer-amount": <int>}` | - |
| **On Request**
| Arduino → Computer | `arduino/out/rotation/current`| rotation payload | - |
| Arduino → Computer | `arduino/out/rotation/velocity` | rotation payload | - |
| Arduino → Computer | `arduino/out/rotation/target` | rotation payload | - |
| Arduino → Computer | `arduino/out/rotation/buffer` | `{"buffer-amount": <int>}` | - |
| **Confirm Messages:**
| Arduino → Computer | `arduino/confirm/rotation/clear` | `{}` | - |
| Arduino → Computer | `arduino/confirm/rotation/target` | `{"buffer-amount": <int>}` | - |
| Arduino → Computer | `arduino/confirm/rotation/path` | `{"buffer-amount": <int>}` | - |
|||||
| Computer → Arduino | `computer/out/rotation/clear` | `{}` | `arduino/confirm/rotation/clear` |
| Computer → Arduino | `computer/out/rotation/path` | path payload | `arduino/confirm/rotation/path` |
| Computer → Arduino | `computer/out/rotation/target` | rotation payload | `arduino/confirm/rotation/target` |
| Computer → Arduino | `computer/out/rotation/info` | `{}` | Arduino must publish `arduino/out/rotation/current`, `arduino/out/rotation/velocity`, `arduino/out/rotation/target` and `arduino/out/rotation/buffer` in response. |

**Rotation status and buffer behavior:**

- After the Computer publishes `computer/out/rotation/path` or `computer/out/rotation/target`, the Arduino immediately confirms that it recieved the request by publishing the corresponding confirm message. 
   - The confirm message has the payload `{"buffer-amount": <int>}` to indicate how full the buffer is.
   - If part of the rotation targets did not fit in the buffer because it filled up. Arduino will add `{"buffer-overflow": <int>}` to the payload indicating how many targets did not fit


### 3. Gripper
| Direction | Topic | Payload | Expected Response |
|-|-|-|-|
| Arduino → Computer | `arduino/out/gripper/state` | gripper payload | - |
| Arduino → Computer | `arduino/out/gripper/complete` | `{}` | - |_
| **Confirm Messages:**
| Arduino → Computer | `arduino/confirm/gripper/target` | `{}` | - |
|||||
| Computer → Arduino | `computer/out/gripper/info`    | `{}` | `arduino/out/gripper/state` |
| Computer → Arduino | `computer/out/gripper/target`  | gripper payload | `arduino/confirm/gripper/target` |

### 4. Health Monitoring

| Direction | Topic | Payload | Expected Response |
|-|-|-|-|
| Arduino → Computer | `arduino/out/health/status` | health payload | - |
|||||
| Computer → Arduino | `computer/out/health/info` | `{}` | `arduino/out/health/status` |


## 2. Payloads
### Position Payloads 
>**Rotation Payload:**  
> ```json
> { "0": <float>, "1": <float>, ..., "5": <float>, "override": <bool> }
> ```
> The keys 0→5 correspond to joints 0→5

> **Path Payload:**  
> ```json
> { "path": [<Rotation Payload>, <Rotation Payload>, ...], "override": <bool> }
> ```

If "override" is true, the current rotation target and any buffered targets are cleared and the new rotation(s) replace them immediately. If "override" is false, the new rotation(s) are appended to the end of the existing buffer.

> **Gripper Payload:**
> ```json
> { "open": <bool> }
> ```
### Status Payloads
> **Health Payload:**  
> ```json
> { "0": <int>, "1": <int>, ..., "5": <int> }
> ```
> |code|description|
> |-|-|
> | **200** | **Health check successfully passed** |
> | 400 | likely Sensor failiure |
> | 401 | likely Motor failiure |

> **Checkup Payload:**  
> ```json
> { "0": <float>, "1": <float>, ..., "5": <float>, "open": <bool> }
> ```

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
5. If the payload can not be paresed to json, a error will be published on the topic it was sent on:
   ```json
   {"error": "payload could not be parsed"}
   ```

# I2C Communication Protocol (Arduino Mega ↔ ATMega328P)

The Arduino Mega communicates with multiple ATMega328P microcontrollers via the I2C bus, acting as the master while the ATMega328Ps serve as slaves.

## Message Format

Each message will consist of `message number` + `payload` in the byte format

| name            | message number | Size (bytes) | Description                          |
|-----------------|----------------|--------------|--------------------------------------|
| rotation target | 0              | 9            | gives a rotation target              |
| stop            | 1              | 1            | force the actuator to imediatly stop |

### `[0]` rotation target

| Field          | Type   | Size (bytes) | Description                       |
|----------------|--------|--------------|-----------------------------------|
| message number | byte   | 1            | message identification number     |
| Delta rotation | float  | 4            | Change in joint angle             |
| Delta time     | float  | 4            | Time elapsed since last update    |

### `[1]` stop

| Field          | Type   | Size (bytes) | Description                       |
|----------------|--------|--------------|-----------------------------------|
| message number | byte   | 1            | message identification number     |

## I2C Address Assignment

Each actuator is assigned a unique I2C address.  
The address corresponds directly to the actuator’s number, starting from the base `0` up to the gripper `5`.

## Notes

- Each ATMega328P receives only the data relevant to its assigned joint.
- The Arduino Mega initiates all communicatn; ATMega328Ps respond only if required by the protocol.