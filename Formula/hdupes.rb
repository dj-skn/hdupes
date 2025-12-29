class Hdupes < Formula
  desc "Fast duplicate file finder with safe defaults and optional actions"
  homepage "https://github.com/dj-skn/hdupes"
  url "https://github.com/dj-skn/hdupes/archive/refs/tags/v1.0.2.tar.gz"
  sha256 "a3d3e7102b92ff80728530f20da30bd3244f59e6d2ede025dd730a670937a082"
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
