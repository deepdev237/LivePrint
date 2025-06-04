# Live Blueprint Collaboration - Configuration Examples

This file shows various configuration examples for different use cases.

## Development/Testing Configuration

```ini
[/Script/LiveBPEditor.LiveBPSettings]
# Enable all features for development
bEnableWirePreviews=true
bEnableNodeLocking=true
bEnableVisualFeedback=true
bEnableNotifications=true
bEnablePerformanceMonitoring=true
bEnableDebugMode=true

# High update rates for responsive development
WirePreviewUpdateRate=20.0
NodeLockTimeout=15.0
NotificationDisplayTime=10.0

# Relaxed throttling for development
MaxMessagesPerSecond=50.0
MaxWirePreviewsPerSecond=20.0
MaxStructuralChangesPerSecond=10.0

# Enable all logging
bEnableVerboseLogging=true
LoggingLevel=VeryVerbose
```

## Production/Release Configuration

```ini
[/Script/LiveBPEditor.LiveBPSettings]
# Essential features only
bEnableWirePreviews=true
bEnableNodeLocking=true
bEnableVisualFeedback=true
bEnableNotifications=true
bEnablePerformanceMonitoring=false
bEnableDebugMode=false

# Conservative update rates for stability
WirePreviewUpdateRate=10.0
NodeLockTimeout=30.0
NotificationDisplayTime=5.0

# Strict throttling for network stability
MaxMessagesPerSecond=20.0
MaxWirePreviewsPerSecond=10.0
MaxStructuralChangesPerSecond=5.0

# Minimal logging
bEnableVerboseLogging=false
LoggingLevel=Warning
```

## High-Performance Configuration (Large Teams)

```ini
[/Script/LiveBPEditor.LiveBPSettings]
# Optimized for many users
bEnableWirePreviews=true
bEnableNodeLocking=true
bEnableVisualFeedback=true
bEnableNotifications=false  # Disable to reduce noise
bEnablePerformanceMonitoring=true
bEnableDebugMode=false

# Reduced update rates for bandwidth efficiency
WirePreviewUpdateRate=5.0
NodeLockTimeout=60.0
NotificationDisplayTime=3.0

# Very strict throttling
MaxMessagesPerSecond=10.0
MaxWirePreviewsPerSecond=5.0
MaxStructuralChangesPerSecond=2.0

# Performance monitoring enabled
bTrackLatency=true
bTrackThroughput=true
bTrackMemoryUsage=true
MaxPerformanceHistorySize=1000

# Minimal logging
bEnableVerboseLogging=false
LoggingLevel=Error
```

## Educational/Training Configuration

```ini
[/Script/LiveBPEditor.LiveBPSettings]
# All visual features for learning
bEnableWirePreviews=true
bEnableNodeLocking=true
bEnableVisualFeedback=true
bEnableNotifications=true
bEnablePerformanceMonitoring=false
bEnableDebugMode=true

# Moderate update rates
WirePreviewUpdateRate=15.0
NodeLockTimeout=45.0
NotificationDisplayTime=8.0

# Moderate throttling
MaxMessagesPerSecond=30.0
MaxWirePreviewsPerSecond=15.0
MaxStructuralChangesPerSecond=8.0

# Enhanced visual feedback
RemoteCursorColor=(R=1.0,G=0.5,B=0.0,A=1.0)
WirePreviewColor=(R=0.0,G=1.0,B=1.0,A=0.8)
NodeLockColor=(R=1.0,G=0.0,B=0.0,A=0.6)

# Verbose logging for learning
bEnableVerboseLogging=true
LoggingLevel=Log
```

## Minimal/Low-Bandwidth Configuration

```ini
[/Script/LiveBPEditor.LiveBPSettings]
# Essential features only, minimal bandwidth
bEnableWirePreviews=false  # Disable wire previews to save bandwidth
bEnableNodeLocking=true
bEnableVisualFeedback=false  # Disable visual feedback
bEnableNotifications=false
bEnablePerformanceMonitoring=false
bEnableDebugMode=false

# Very conservative settings
WirePreviewUpdateRate=1.0
NodeLockTimeout=120.0
NotificationDisplayTime=2.0

# Extreme throttling
MaxMessagesPerSecond=2.0
MaxWirePreviewsPerSecond=1.0
MaxStructuralChangesPerSecond=1.0

# No logging
bEnableVerboseLogging=false
LoggingLevel=Fatal
```

## Testing/QA Configuration

```ini
[/Script/LiveBPEditor.LiveBPSettings]
# All features enabled for comprehensive testing
bEnableWirePreviews=true
bEnableNodeLocking=true
bEnableVisualFeedback=true
bEnableNotifications=true
bEnablePerformanceMonitoring=true
bEnableDebugMode=true

# Standard rates
WirePreviewUpdateRate=10.0
NodeLockTimeout=30.0
NotificationDisplayTime=5.0

# Standard throttling
MaxMessagesPerSecond=20.0
MaxWirePreviewsPerSecond=10.0
MaxStructuralChangesPerSecond=5.0

# Full performance monitoring
bTrackLatency=true
bTrackThroughput=true
bTrackMemoryUsage=true
bTrackActiveUsers=true
MaxPerformanceHistorySize=5000

# Testing framework enabled
bEnableTestFramework=true
TestingUpdateInterval=1.0

# Full logging
bEnableVerboseLogging=true
LoggingLevel=VeryVerbose
```

## Console Command Reference

### Basic Commands
- `LiveBP.Help` - Show all available commands
- `LiveBP.Debug.ShowStats` - Display current performance statistics
- `LiveBP.Debug.TestConnection` - Test Multi-User Editing connection status

### Debug Commands  
- `LiveBP.Debug.ToggleDebugMode` - Toggle debug visualization on/off
- `LiveBP.Debug.ClearLocks` - Clear all node locks (admin only)
- `LiveBP.Debug.SimulateLatency 100` - Simulate 100ms network latency
- `LiveBP.Debug.RunTests` - Execute the full test suite

### Development Commands
- `LiveBP.Debug.DumpMessages` - Show recent collaboration messages
- `stat LiveBP` - Show detailed performance statistics
- `showdebug LiveBP` - Toggle on-screen debug information

## Performance Tuning Guidelines

### For Small Teams (2-4 users):
- Wire Preview Rate: 15-20 Hz
- Node Lock Timeout: 15-30 seconds  
- Message Throttling: 30-50 msg/s
- Enable all visual feedback

### For Medium Teams (5-8 users):
- Wire Preview Rate: 10-15 Hz
- Node Lock Timeout: 30-45 seconds
- Message Throttling: 15-30 msg/s
- Selective visual feedback

### For Large Teams (9+ users):
- Wire Preview Rate: 5-10 Hz
- Node Lock Timeout: 45-60 seconds
- Message Throttling: 5-15 msg/s
- Minimal visual feedback

### Network Optimization:
- Use binary serialization for wire previews
- Compress repeated messages
- Implement message deduplication
- Use adaptive throttling based on network conditions
