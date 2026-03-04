# Maintainer: YOUR NAME <your@email.com>
pkgname=vml
pkgver=1.0.0
pkgrel=1
pkgdesc="Real-time voice modulation with virtual microphone via PipeWire"
arch=('x86_64')
url="https://github.com/zebratic/vml"
license=('GPL-3.0-only')
depends=('qt6-base' 'pipewire')
makedepends=('cmake' 'gcc')
source=("$pkgname-$pkgver.tar.gz::$url/archive/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
    cmake -B build -S "$pkgname-$pkgver" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build -j"$(nproc)"
}

package() {
    DESTDIR="$pkgdir" cmake --install build
}
