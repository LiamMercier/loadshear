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

TODO:

## PORT

TODO:

## HEADERSIZE

TODO: 

## BODYMAX

TODO:

## READ

TODO:

## REPEAT

TODO:

## ENDPOINTS

TODO:

## SHARDS

TODO:

## PACKETS

TODO:

## HANDLER

TODO:

