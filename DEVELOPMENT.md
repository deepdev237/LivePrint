# Live Blueprint Collaboration Plugin - Development Guide

## Building the Plugin

### Prerequisites
- Unreal Engine 5.5+
- Visual Studio 2022 (Windows) or Xcode (Mac)
- Multi-User Editing plugin enabled in project

### Build Configuration
1. Place the plugin in your project's `Plugins/` directory
2. Regenerate project files
3. Build in Development Editor configuration

### Testing
The plugin includes a comprehensive test framework accessible via:
- Editor menu: Tools > Live Blueprint > Run Tests
- C++ API: `FLiveBPTestFramework::Get().RunAllTests()`

### Debug Console Commands
- `LiveBP.Debug.ShowStats` - Display performance statistics
- `LiveBP.Debug.TestConnection` - Test MUE connection
- `LiveBP.Debug.ClearLocks` - Clear all node locks
- `LiveBP.Debug.SimulateLatency [ms]` - Simulate network latency

## Architecture Deep Dive

### Message Flow
1. User performs Blueprint operation (wire drag, node move, etc.)
2. `LiveBPEditorSubsystem` captures operation
3. Message serialized via `LiveBPUtils`
4. `LiveBPMessageThrottler` applies rate limiting
5. `LiveBPMUEIntegration` sends via Concert transport
6. Remote clients receive and deserialize
7. `LiveBPLockManager` handles conflicts
8. Visual feedback applied via `SLiveBPGraphEditor`

### Performance Optimization
- Wire previews: 10Hz update rate (configurable)
- Structural changes: Sub-50ms target latency
- Message compression: Binary format for wire data
- Lock expiry: 30 second timeout (configurable)
- Memory pooling: Reused message objects

### Conflict Resolution
1. **Optimistic Locking**: Users can start operations immediately
2. **Lock Acquisition**: First-come-first-served node locking
3. **Conflict Detection**: Automatic detection of simultaneous edits  
4. **Resolution**: FIFO queue for conflicting operations
5. **User Feedback**: Visual indicators and notifications

## Customization

### Settings Configuration
All settings available in Editor Preferences > Live Blueprint:
- Performance thresholds and update rates
- Visual feedback options
- Debug and logging levels
- Message throttling parameters

### Extending the System
- Add new message types in `LiveBPDataTypes.h`
- Implement handlers in `LiveBPEditorSubsystem`
- Add serialization support in `LiveBPUtils`
- Update visual feedback in `SLiveBPGraphEditor`

## Troubleshooting

### Common Issues
1. **MUE Connection Failed**: Ensure Concert is running and Multi-User plugin enabled
2. **High Latency**: Check network and adjust throttling settings
3. **Lock Conflicts**: Increase lock timeout or implement priority system
4. **Visual Glitches**: Verify graph editor coordinate transformations

### Debug Logging
Enable verbose logging via:
```cpp
UE_LOG(LogLiveBPCore, VeryVerbose, TEXT("Debug message"));
```

Categories: `LogLiveBPCore`, `LogLiveBPEditor`

## Future Enhancements

### Planned Features
- Blueprint diffing and merging
- Undo/redo synchronization  
- Voice chat integration
- Persistent collaboration sessions
- Mobile companion app

### Known Limitations
- Maximum 16 concurrent users
- Large graphs (1000+ nodes) may impact performance
- Some Blueprint features not yet supported (Custom Events, etc.)
- Cross-engine version compatibility not guaranteed
