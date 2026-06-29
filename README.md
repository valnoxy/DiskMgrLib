# DiskMgrLib

C++ Library for disk management. 
This library was developed primarily to simplify disk management in my C# application [Dive](https://github.com/valnoxy/Dive). Since I have other projects that also rely on these functions, I've moved this library to a separate repository

See **diskengine.h** for all exported functions

## Usage Examples

```csharp
private const string DllPath = "DiskMgrLib.dll";

[DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
private static extern bool DiskMgr_CreateGPTPartition(int diskNumber, ulong sizeInMB, Guid partitionType);

private static readonly Guid GUID_BASIC_DATA = new("ebd0a0a2-b9e5-4433-87c0-68b6b72699c7");

if (!DiskMgr_CreateGPTPartition(1, 5000, GUID_BASIC_DATA))
{
    Console.WriteLine("Failed to create gpt partition")
    return false;
}
```
[Code example](https://github.com/valnoxy/Dive/blob/main/Dive/Dive.Core/Action/DiskPreparation/GptDiskManager.cs)