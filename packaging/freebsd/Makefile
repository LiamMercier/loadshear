PORTNAME=	loadshear
DISTVERSION=	1.0.0
CATEGORIES=	net
WWW=		https://github.com/LiamMercier/loadshear

MAINTAINER=	LiamMercier@proton.me
COMMENT=	a customizable load generator
LICENSE=	GPLv3
LICENSE_FILE=	${WRKSRC}/LICENSE

USES=		cmake compiler:c++23-lang
LIB_DEPENDS=	boost-all
BUILD_DEPENDS=	rustc:lang/rust

USE_GITHUB=	yes
GH_ACCOUNT=	LiamMercier
GH_PROJECT=	loadshear
GH_TAGNAME=	v1.0.0

.include <bsd.port.mk>
