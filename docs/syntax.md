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

These are not values, they are parsed as identifiers.

- seconds 
- little 
- packet_name

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

### Fields

TODO: table of fields

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

Values are connectable endpoints, separated by commas.

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

With comma's between multiple packets.

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

You can simply read and drop packets using "NOP" or you can provide your own packet response handler by compiling a WebAssembly module. Instructions for compiling the module are in the main README.

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

