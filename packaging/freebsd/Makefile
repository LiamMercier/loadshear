PORTNAME=	loadshear
DISTVERSION=	1.0.0
CATEGORIES=	net
WWW=		https://github.com/LiamMercier/loadshear

MAINTAINER=	LiamMercier@proton.me
COMMENT=	a customizable load generator
LICENSE=	GPLv3
LICENSE_FILE=	${WRKSRC}/LICENSE

USES=		cmake compiler:c++23-lang
LIB_DEPENDS=	libboost_system.so:devel/boost-libs libboost_program_options.so:devel/boost-libs
BUILD_DEPENDS=	rustc:lang/rust

USE_GITHUB=	yes
GH_ACCOUNT=	LiamMercier
GH_TAGNAME=	v1.0.0

GH_TUPLE=	bytecodealliance:wasmtime:v40.0.0:wasmtime/third_party/wasmtime \
		ArthurSonzogni:FTXUI:v6.1.9:ftxui/deps/ftxui

CXXFLAGS+=	-fexperimental-library -flto -O2
LDFLAGS+=	-flto

CMAKE_ARGS+=	-DFETCHCONTENT_SOURCE_DIR_FTXUI=${WRKSRC}/deps/ftxui \
		-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON

INSTALL_TARGET= install/strip

.include <bsd.port.mk>
