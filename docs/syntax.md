# Loadshear DSL Syntax

## Comments

All comments are started with C-style slashes `//` and extend until the end of the current line.

## Values

Values in scripts must either start with a number, or be quoted. Text that is not quoted is treated as an identifier and not a value.

### Examples

These are values

- "true"
- 25
- 5ms
- "little"

These are not values

- seconds 
- little 
- packet_name

## Keywords

Keywords are in all capital letters. Every keyword is listed in this document.

## Identifiers

Identifiers are text starting with alphabetical characters, which are not keywords.

### Examples

These are identifiers

- packet_1
- mysettings
- scriptpacket
- a

These are not identifiers

- HANDLER (or other keywords)
- 1packet
- "seconds"
- "1packet"

## SETTINGS

The SETTINGS block sets options for execution plan generation.

### Fields

| Field                     | Value          | Optional  | Default  | 
|---------------------------|----------------|-----------|----------|
| [SESSION](#SESSION)       | enum string    | Required  | None     |
| [PORT](#PORT)             | integer        | Required  | Depends  |
| [HEADERSIZE](#HEADERSIZE) | integer        | Depends   | None     |
| [BODYMAX](#BODYMAX)       | integer        | Depends   | None     |
| [READ](#READ)             | boolean        | Optional  | "false"  |
| [REPEAT](#REPEAT)         | boolean        | Optional  | "false"  |
| [ENDPOINTS](#ENDPOINTS)   | list\<string\> | Required  | None     |
| [SHARDS](#SHARDS)         | integer        | Optional  | Depends  |
| [PACKETS](#PACKETS)       | list\<string\> | Depends   | None     |
| [HANDLER](#HANDLER)       | string         | Depends   | "NOP"    |

### Usage

```
SETTINGS setting_identifier {
    // Fields go here.
}
```

## ORCHESTRATOR

The ORCHESTRATOR block defines the order of actions to include in the execution plan.

We parse the actions linearly to construct a list of actions taken by the orchestrator at runtime. Certain actions are required for others to be valid (i.e CREATE before CONNECT). On the final action, the orchestrator will do the action and set all resources as being able to shutdown and issue closing commands.

### Fields

| Field                     | Optional |
|---------------------------|----------|
| [CREATE](#CREATE)         | Required |
| [CONNECT](#CONNECT)       | Required |
| [SEND](#SEND)             | Optional |
| [FLOOD](#FLOOD)           | Optional |
| [DRAIN](#DRAIN)           | Optional |
| [DISCONNECT](#DISCONNECT) | Optional |

### Usage

```
ORCHESTRATOR settings_identifier {
    // Actions go here.
}
```

## SESSION

The type of session used for connections. 

### Values

SESSION may be any of the following values

- "TCP"

### Usage

```
{
    ...
    SESSION = "TCP"
    ...
}
```

[back](#fields)

## PORT

The port used for connections. Only set to a default value if possible for the session type.

### Values

PORT must be a valid integer from 1 through 65535

### Usage

```
{
    ...
    PORT = 12345
    ...
}
```

[back](#fields)

## HEADERSIZE

The number of bytes a session must read to construct a header for extracting the body length of the packet.

### Values

Optional if READ is "false"

HEADERSIZE must be a positive integer

### Usage

```
{
    ...
    HEADERSIZE = 16
    ...
}
```

[back](#fields)

## BODYMAX

The maximum body size for an incoming packet. Sessions will close if they see a header that asks to read more bytes than BODYMAX allows.

### Values

Optional if READ is "false"

BODYMAX must be a positive integer

### Usage

```
{
    ...
    BODYMAX = 12288
    ...
}
```

[back](#fields)

## READ

Decide if sessions should read data from the socket.

### Values

READ must be "true" or "false"

### Usage

```
{
    ...
    READ = "true"
    ...
}
```

[back](#fields)

## REPEAT

Decide if sessions should repeat payloads defined by the ORCHESTRATOR block once they have delivered all of their data.

### Values

REPEAT must be "true" or "false"

### Usage

```
{
    ...
    REPEAT = "true"
    ...
}
```

[back](#fields)

## ENDPOINTS

A list of endpoints that sessions will try to resolve for connecting to the target. 

These should all be for the same logical target, listing multiple logical targets can result in load generation being split between servers based on how sessions decide to connect.

### Values

Connectable endpoints, separated by commas.

### Usage

```
{
    ...
    ENDPOINTS {
        "localhost",
        127.0.0.1
    }
    ...
}
```

[back](#fields)

## SHARDS

The number of single threaded shards to distribute work across.

### Values

SHARDS is optional and must be a positive value, less than or equal to the number of sessions being created.

By default, SHARDS will be set to the detected number of threads that can be concurrently executed. Typically SHARDS will equal the number of logical cores in your device.

### Usage

```
{
    ...
    SHARDS = 8
    ...
}
```

[back](#fields)

## PACKETS

The list of packets that will be read by the program to create payloads.

### Values

Each packet should be listed as

```
<Identifier> : <Value>
```

With commas between multiple packets.

### Usage

```
{
    PACKETS {
        packet_1 : "path/to/packet.bin",
        packet_2 : "path/to/packet2.bin"
    }
}
```

[back](#fields)

## HANDLER

The type of message handler to use in each session for handling packets we read.

### Values

You can simply read and drop packets using "NOP" or you can provide your own packet response handler by compiling a WebAssembly module. Instructions for compiling a compatible module are available in [wasm-modules](wasm-modules.md).

- "NOP"
- "path/to/wasm/module.wasm"

### Usage

```
{
    ...
    HANDLER = "modules/mymodule.wasm"
    ...
}
```

[back](#fields)

## CREATE

Create N sessions ready to do future tasks. Each ORCHESTRATOR should start with one CREATE action before any other work is scheduled.

The sessions are split equally across each shard, with rounding. So, each shard will get around (N / SHARDS) sessions, with index values sequentially added to the shards one by one

### Usage

CREATE takes an integer number of sessions and optionally an [OFFSET](#OFFSET) field.

```
CREATE <num_sessions:int> [OFFSET <value:time>]
```

```
SETTINGS {
    // ... more fields here
    SHARDS = 2
}

ORCHESTRATOR settings_id {
    CREATE 100 OFFSET 0ms
}
```

Creates 100 sessions, index 0 through 49 on shard 1, 50 through 99 on shard 2.

[back](#fields-1)

## CONNECT

Connect the session range to the endpoints.

### Usage

CONNECT will try to connect sessions \[start, end) to the endpoint. We can schedule the action using an optional [OFFSET](#OFFSET) field.

```
CONNECT <start:int>:<end:int> [OFFSET <value:time>]
```

```
ORCHESTRATOR settings_id {
    CONNECT 0:100 OFFSET 100ms
}
```

Connects sessions indexed 0 through 99 to the endpoint.

[back](#fields-1)

## SEND

Schedule a payload to be sent from sessions to the endpoint.

### Usage

Send M copies of a packet from sessions \[start, end) to the endpoint, scheduled using an optional [OFFSET](#OFFSET) field.

We can modify the packet using [COUNTER](#COUNTER) and [TIMESTAMP](#TIMESTAMP)

```
ORCHESTRATOR settings_id {
    CREATE 100 OFFSET 0ms
    CONNECT 0:100 OFFSET 100ms
    SEND 0:100 p2 COPIES 1 COUNTER 0:8 "little":7 TIMESTAMP 12:8 "big":"milliseconds" OFFSET 200ms
}
```

- Send 1 copy of packet identifier p2 from sessions 0 through 99 to the endpoint
    - With 8 bytes replaced starting at index 0 by a little endian counter using a session global increment of 7
    - With 8 bytes replaced starting at index 12 by a big endian timestamp of milliseconds

[back](#fields-1)
    
## FLOOD

Set session range to flood with any scheduled payloads. SEND actions after FLOOD simply add to the payloads we prepare at program start, but do not control when we send the payload (FLOOD overrides their actions and says to send as soon as possible).

### Usage

FLOOD takes a session range \[start, end) and optionally an [OFFSET](#OFFSET) field.

```
FLOOD <start:int>:<end:int> [OFFSET <value:time>]
```

```
ORCHESTRATOR settings_id {
    CREATE 100 OFFSET 0ms
    CONNECT 0:100 OFFSET 100ms
    // ...
    FLOOD 0:100 OFFSET 500ms
}
```

[back](#fields-1)

## DRAIN

Tell every session to disconnect after they are finished with their payloads, if REPEAT is "true" we will not go into the next payload loop.

### Usage

DRAIN takes a session range \[start, end) and optionally an [OFFSET](#OFFSET) field.

```
DRAIN <start:int>:<end:int> [OFFSET <value:time>]
```

```
ORCHESTRATOR settings_id {
    CREATE 100 OFFSET 0ms
    CONNECT 0:100 OFFSET 100ms
    // ...
    DRAIN 0:100 OFFSET 500ms
}
```

[back](#fields-1)

## DISCONNECT

Forcibly disconnect each session in the range. This should be called as the end of an ORCHESTRATOR block if you wish for DRAIN to give each session some time, since the orchestrator will try to exit after the last action.

### Usage

DISCONNECT takes a session range \[start, end) and optionally an [OFFSET](#OFFSET) field.

```
DISCONNECT <start:int>:<end:int> [OFFSET <value:time>]
```

```
ORCHESTRATOR settings_id {
    CREATE 100 OFFSET 0ms
    CONNECT 0:100 OFFSET 100ms
    // ...
    DRAIN 0:100 OFFSET 500ms
}
```

[back](#fields-1)

## OFFSET

Schedule the action relative to the previous action.

### Values

We take values for time, values may be a quoted integer with s or ms appended. For actions which do not define an OFFSET, we implicitly provide one.

### Usage

```
CREATE 100 OFFSET 20s
```

```
DRAIN 0:100 OFFSET 500ms
```

[back](#fields-1)

## COUNTER

Create a global counter with an increment for sessions to use when sending this payload. There may only be one instance of COUNTER per SEND declaration.

Each copy of the payload will increment the counter by the increment size, and each session will increment the same counter.

Using COUNTER can reduce throughput, when two shards need to update the same counter there will be cache line contention because both threads are writing to the same memory.

### Values

The start and length values must be positive integers. Start must be a valid index into the packet bytes, length must be at most 8.

The endian options are "little" and "big"

Increments must be integers.

### Usage

```
COUNTER <start:int>:<length:int> <time format:string>:<increment:int>
```

```
...
    SEND 0:100 username COPIES 1 COUNTER 0:8 "little":1
...
```

Modify username to have a counter of length 8 starting at index 0 with global increment of 1 in little endian format.

[back](#fields-1)

## TIMESTAMP

Replace bytes of the packet with a timestamp.

### Values

The start and length values must be positive integers. Start must be a valid index into the packet bytes, length must be at most 8.

The endian options are "little" and "big"

The timestamp format must be "seconds", "milliseconds", "microseconds", or "nanoseconds"

### Usage

```
TIMESTAMP <start:int>:<length:int> <time format:string>:<increment:int>
```

```
...
    SEND 0:100 p1 COPIES 1 TIMESTAMP 0:8 "little":"seconds" OFFSET 200ms
...
```

Modify p1 to have a timestamp in seconds of length 8 starting at index 0 in little endian format.

[back](#fields-1)
