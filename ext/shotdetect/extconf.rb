require 'mkmf'
extension_name = 'shotdetect'

HEADER_DIRS = [
  # First search /opt/local for macports
  '/opt/local/include',
  # Then search /usr/local for people that installed from source
  '/usr/local/include',
  # Finally fall back to /usr
  '/usr/include',
]

LIB_DIRS = [
  # First search /opt/local for macports
  '/opt/local/lib',
  # Then search /usr/local for people that installed from source
  '/usr/local/lib',
  # Finally fall back to /usr
  '/usr/lib',
]

dir_config(extension_name, HEADER_DIRS, LIB_DIRS)

# Check header files
unless find_header('libswscale/swscale.h')
  abort "!!! Please install libswscale-dev"
end

unless find_header('libavcodec/avcodec.h')
  abort "!!! Please install libavcodec-dev"
end

unless find_header('libavformat/avformat.h')
  abort "!!! Please install libavformat-dev"
end

unless find_header('libavutil/avutil.h')
  abort "!!! Please install libavutil-dev"
end

have_library('avutil')
have_library('avcodec')
have_library('avformat')
have_library('swscale')

create_makefile(extension_name)
