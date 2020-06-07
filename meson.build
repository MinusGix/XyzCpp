project(
    'XyzCpp',
    'c', 'cpp',
    default_options : ['cpp_std=c++17', 'c_std=c11', 'buildtype=debugoptimized'],
    license : 'MIT',
    version : '0.0.5'
)

cxx = meson.get_compiler('cpp')

thread_dep = dependency('threads')
zlib_dep = dependency('zlib')
# it was not being found via normal means on my system
boost_iostreams_dependency = cxx.find_library('boost_iostreams')
openssl_dep = dependency('openssl')
deps = [
    thread_dep,
    zlib_dep,
    dependency('readline'),
    boost_iostreams_dependency,
    openssl_dep,
]

src = []


#libxyzcpp = shared_library(
#    'xyzcpp',
#    [],
#    include_directories : include_directories('include'),
#    dependencies : deps,
#    install : false
#)

test_client = executable('test_client', sources : ['src/test_client.cpp'], dependencies : deps)
test('test_client', test_client)
test_server = executable('test_server', sources : ['src/test_server.cpp'], dependencies : deps)
test('test_server', test_server)
libxyzcpp_dep = declare_dependency(
    include_directories : include_directories('include'),
    dependencies : deps
)