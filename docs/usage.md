# Usage Guide

## Designing Tests

Users are expected to create a Loadshear script and any other necessary data for running the application. For complex use cases, consult the main README, syntax reference, or the WASM documentation.

Users should be aware that each Session will require a socket. For large tests, you may need to increase kernel limits such as file descriptors or ephemeral ports.

Loadshear collects client-side metrics for data throughput, connection latency, etc. However, many of these measurements cannot be used to determine end-to-end metrics. For a proper evaluation, it is advised to collect server side metrics to complement the client side metrics for any thorough analysis of performance.

Loadshear has no proxy support by design, you may need to change firewall or server settings to allow multiple sessions.

## Metrics

Every 500ms, a snapshot of the collected metrics is displayed in the terminal user interface. Increments can be shown instead of totals if selected with the left and right arrow keys. 

Loadshear collects the following metrics:

### Throughput 

#### Bytes Sent

This measures the number of bytes copied from the program into the kernel's send buffer. Each time a packet is successfully given to the kernel, this counter is incremented by the bytes copied.

#### Bytes Read

This measures the number of bytes read from the kernel's receive buffer. Each time we read from a socket, this is incremented.

### Connections

#### Active

The number of session objects that are active in the session pool. (In a valid state after created, or currently connected).

#### Attempted

The number of connection attempts we have made.

#### Failed

The number of connection attempts that have resulted in a failure.

#### Successful

The number of connection attempts that have been successful.

### Histograms

Each histogram bin is labeled using its exclusive upper bound (not including the last bin). Each upper bound is twice as large as the last, starting at 64 microseconds.

For example, the label `64 us` represents the bin of latencies less than 64 microseconds, and the next bin has the label `128 us` representing latencies between 64 and 127 microseconds. The last bin with the label `>1 s` holds values between 1 second and greater.

#### Connection Latency

This measures the time from when we try to connect to when we get a successful connection. This does not take into account failed connections.

#### Send Latency

This measures the time from when we try to send a packet to when the kernel accepts our data. This does not measure when the packet is transmitted over the network, but high send latency can indicate that the server or network stack is having trouble accepting data at the current throughput.

#### Read Latency

This measures the time from when we first tried to read to when we are given a valid packet. This can be useful if your server implements responses and you send requests, but otherwise these analytics might be unimportant.
