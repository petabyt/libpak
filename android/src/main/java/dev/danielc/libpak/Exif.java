package dev.danielc.libpak;

public class Exif {
    public native static byte[] getExifThumbnail(String filepath);
}
