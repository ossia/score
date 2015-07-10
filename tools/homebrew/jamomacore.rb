require "formula"

class JamomaCore < Formula
  homepage "http://www.jamoma.org/"

  head "https://github.com/Jamoma/JamomaCore.git", :branch => "dev"
  depends_on "cmake" => :build

  def install
    args = std_cmake_args

    system "cmake", ".", *args
    system "make", "install"
    prefix.install "install_manifest.txt"
  end
end