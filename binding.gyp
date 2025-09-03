{
  "targets": [
    {
      "target_name": "audio_capture",
      "sources": [
        "src/addon_main.cc",
        "src/mac/mac_impl.mm",
        "src/mac/utils.cc",
        "src/mac/process_list.mm",
        "src/mac/audio_tap.mm"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "include",
        "include/mac"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": [
        "NODE_ADDON_API_CPP_EXCEPTIONS"
      ],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "14.4",
        "OTHER_LDFLAGS": [
          "-framework CoreAudio",
          "-framework AudioToolbox",
          "-framework AppKit",
          "-framework Foundation",
          "-framework AVFoundation",
          "-framework CoreFoundation"
        ]
      }
    }
  ]
}