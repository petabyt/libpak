use libpak_sys::*;
use std::mem::{align_of, offset_of, size_of};
use std::process::Command;

#[test]
fn selected_c_layouts_and_constants_match() {
    let root = std::path::Path::new(env!("CARGO_MANIFEST_DIR"));
    let output = std::env::temp_dir().join(format!("libpak-abi-probe-{}", std::process::id()));
    let cc = std::env::var_os("CC").unwrap_or_else(|| "cc".into());
    let status = Command::new(cc).arg("-std=c11").arg("-O1").arg("-D_DEFAULT_SOURCE")
        .arg("-I").arg(root.join("../include"))
        .arg(root.join("tests/abi_probe.c")).arg("-o").arg(&output).status().unwrap();
    assert!(status.success(), "could not compile C ABI probe");
    let result = Command::new(&output).output().unwrap();
    std::fs::remove_file(&output).unwrap();
    assert!(result.status.success(), "C ABI probe failed");
    let c: Vec<usize> = String::from_utf8(result.stdout).unwrap().split_whitespace()
        .map(|number| number.parse().unwrap()).collect();
    let rust = vec![
        size_of::<PakModule>(), align_of::<PakModule>(), offset_of!(PakModule, rt),
        offset_of!(PakModule, on_try_connect_usb), offset_of!(PakModule, on_request_liveview_frame),
        offset_of!(PakModule, on_custom_command), size_of::<PakWidget>(), align_of::<PakWidget>(),
        offset_of!(PakWidget, u), size_of::<PakFileHandle>(), size_of::<PakSavedConnection>(),
        size_of::<PakWiFiApFilter>(), size_of::<PakBtDevice>(),
        PakScreen_PAK_SCREEN_LIVEVIEW as usize, PakTransport_PAK_USB as usize,
    ];
    assert_eq!(c, rust);
}
