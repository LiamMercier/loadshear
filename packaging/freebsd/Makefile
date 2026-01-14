PORTNAME=	loadshear
DISTVERSION=	1.0.0
CATEGORIES=	net
MAINTAINER=	LiamMercier@proton.me
WWW=		https://github.com/LiamMercier/loadshear

COMMENT=	a customizable load generator
LICENSE=	GPLv3
LICENSE_FILE=	${WRKSRC}/LICENSE

USES=		cmake compiler:c++23-lang
LIB_DEPENDS=	boost-all
BUILD_DEPENDS=	rustup-init

USE_GITHUB=	yes
GH_ACCOUNT=	LiamMercier
GH_PROJECT=	loadshear

.include <bsd.port.mk>
