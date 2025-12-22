# Loadshear DSL Syntax

## SETTINGS

The SETTINGS block sets options for execution plan generation.

### Fields

| Field      | Value       | Optional  | Default | 
|------------|-------------|-----------|---------|
| SESSION    | Enum String | Required  | N/A     |
| PORT       | Integer     | Required  | Depends |

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
