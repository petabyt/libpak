package dev.danielc.libpak;

public class Pak {
    public enum Error {
        PERMISSION_DENIED(-1),
        UNSUPPORTED(-2),
        UNIMPLEMENTED(-3),
        NOT_CONNECTED(-4);

        private final int code;

        Error(int code) {
            this.code = code;
        }

        public int getCode() {
            return code;
        }
    }
}
