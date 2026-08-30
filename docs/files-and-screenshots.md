# Files and screenshots

Xbox paths use a drive prefix and backslashes, for example
`Hdd:\\Content\\file.bin` in a C++ string literal.

## Directory and in-memory file operations

```cpp
const auto drives = client.drives();
const auto entries = client.directory_contents("Hdd:\\Content");

client.create_directory("Hdd:\\Tools");
client.rename_path("Hdd:\\old.bin", "Hdd:\\new.bin");
client.delete_file("Hdd:\\new.bin");
client.delete_directory("Hdd:\\Tools");
```

Each `DirectoryEntry` contains the name, byte size, directory flag, and
optional creation/change FILETIME values.

Small files can be kept in memory:

```cpp
const auto data = client.download_file("Hdd:\\settings.bin");
client.upload_file("Hdd:\\settings-copy.bin", data, true);
```

`download_file()` defaults to a 512 MiB safety limit; pass a deliberate higher
limit when required. The `true` upload argument permits overwrite. XBDM
implements overwrite as delete-then-retry, so it is not atomic and a failed
upload can leave the destination absent.

## Streaming one file

Local-path operations avoid holding the whole file in memory and report
progress:

```cpp
srpc::FileTransferOptions options;
options.existing_file = srpc::ExistingFilePolicy::overwrite;
options.maximum_file_size = 1024ULL * 1024 * 1024;
options.progress = [](const srpc::TransferProgress& progress) {
    std::cout << progress.file_bytes_transferred << " / "
              << progress.file_bytes_total << '\n';
    return srpc::TransferControl::continue_;
};

const auto result = client.download_file_to(
    "Hdd:\\settings.bin",
    "settings.bin",
    options);
```

The matching upload method is:

```cpp
const auto result = client.upload_file_from(
    "settings.bin",
    "Hdd:\\settings.bin",
    options);
```

`ExistingFilePolicy` is `fail`, `overwrite`, or `skip`. The default is
`fail`. `FileTransferOptions` also controls chunk size, maximum file size, and
whether an incomplete local download is removed.

Returning `TransferControl::cancel` produces a normal `TransferResult` with
`cancelled == true`. If cancellation interrupts a binary frame, the client
closes its connection so unread bytes cannot corrupt a later response. Call
`reconnect()` before another operation when the connection is closed.

A download is first written to a sibling temporary file and committed only
after the complete body arrives. The destination's parent directory must
already exist for a single-file download.

The progress callback owns the active exchange and must not call another
method on the same `Client`.

## Recursive directories

```cpp
srpc::DirectoryTransferOptions options;
options.files.existing_file = srpc::ExistingFilePolicy::overwrite;
options.maximum_depth = 32;
options.maximum_entries = 50'000;
options.maximum_total_size = 4ULL * 1024 * 1024 * 1024;

const auto result = client.download_directory_to(
    "Hdd:\\Content",
    "Content",
    options);
```

Use `upload_directory_from()` for the reverse direction. Recursive transfers
reject traversal components, separator injection, nested local symlinks or
reparse points, and configured depth/entry/size overflows. Completed earlier
files remain if a later file fails or is cancelled; a tree transfer does not
provide global rollback.

## Screenshots

The simplest operation captures, decodes, and writes a PNG to the exact path
provided by the application:

```cpp
client.save_screenshot("screenshot.png");
```

The parent directory must already exist. For an in-memory result:

```cpp
const auto image = client.capture_screenshot();
const srpc::ByteBuffer png = srpc::encode_png(image.view());
```

For a custom decoder or archival pipeline, retain the XBDM metadata and tiled
framebuffer:

```cpp
const auto raw = client.capture_raw_screenshot();
```

Decoded images contain tightly packed BGRA8 pixels. A8R8G8B8 and packed
A2R10G10B10 frontbuffers are supported. The default preserves the visible
frontbuffer dimensions, uses the Xenos 2D untile mode, and emits opaque alpha.

```cpp
srpc::ScreenshotOptions options;
options.compose_display_surface = true;
options.preserve_alpha = false;
options.maximum_framebuffer_size = 64 * 1024 * 1024;
options.maximum_decoded_size = 64 * 1024 * 1024;

const auto image = client.capture_screenshot(options);
```

`compose_display_surface` applies XBDM's display dimensions and offsets. The
`morton` untile mode exists only for compatibility with captures produced by
older tooling. Keep `xenos` for normal XBDM screenshots.

`encode_png()` and `write_png()` also accept a caller-created `ImageView` in
RGBA8 or BGRA8 format. The encoder has no external image dependency and does
not create parent directories.
