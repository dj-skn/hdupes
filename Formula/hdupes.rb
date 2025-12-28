class Hdupes < Formula
  desc "Fast duplicate file finder with safe defaults and optional actions"
  homepage "https://github.com/dj-skn/hdupes"
  url "https://github.com/dj-skn/hdupes/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "bb2f6eafb7534e482275af7757a3ca668bcd618ec26a6fa69b6ea651a0d897de"
  license "MIT"

  depends_on "libjodycode"

  def install
    ENV.append "CFLAGS_EXTRA", " -I#{Formula["libjodycode"].opt_include}"
    ENV.append "LDFLAGS_EXTRA", " -L#{Formula["libjodycode"].opt_lib}"

    system "make", "ENABLE_DEDUPE=1"
    system "make", "install", "PREFIX=#{prefix}"
  end

  test do
    touch "a"
    touch "b"
    (testpath/"c").write("unique file")
    dupes = shell_output("#{bin}/hdupes --zero-match .").strip.split("\n").sort
    assert_equal ["a", "b"], dupes
  end
end
