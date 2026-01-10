#ifndef BDB_OPTIONS_INCLUDED
#define BDB_OPTIONS_INCLUDED

#define BDB_SUPPORT_NWK_STEERING (1)

/* bdbcTLPrimaryChannelSet */
#define BDBC_TL_PRIMARY_CHANNEL_SET (0x02108800)
/* bdbcTLSecondaryChannelSet */
#define BDBC_TL_SECONDARY_CHANNEL_SET (0x07fff800 ^ BDBC_TL_PRIMARY_CHANNEL_SET)

/* BDB Constants */
#define BDBC_IMP_MAX_REJOIN_CYCLES (10)
/* bdbcMaxSameNetworkRetryAttempts */
#define BDBC_MAX_SAME_NETWORK_RETRY_ATTEMPTS (10)
/* bdbcMinCommissioningTime */
#define BDBC_MIN_COMMISSIONING_TIME (180)
/* bdbcRecSameNetworkRetryAttempts */
#define BDBC_REC_SAME_NETWORK_RETRY_ATTEMPTS (3)
/* bdbcTCLinkKeyExchangeTimeout */
#define BDBC_TC_LINK_KEY_EXCHANGE_TIMEOUT (5)

/* BDB Attribute values */
/* bdbCommissioningGroupID */
#define BDB_COMMISSIONING_GROUP_ID (0xFFFF)
/* bdbCommissioningMode */
#define BDB_COMMISSIONING_MODE (BDB_COMMISSIONING_MODE_NWK_STEERING)
/* bdbCommissioningStatus */
#define BDB_COMMISSIONING_STATUS (0x00)
/* bdbJoiningNodeEui64 */
#define BDB_JOINING_NODE_EUI64 (0x0000000000000000)
/* bdbJoinUsesInstallCodeKey */
#define BDB_JOIN_USES_INSTALL_CODE_KEY (FALSE)

/* bdbNodeJoinLinkKeyType */
#define BDB_NODE_JOIN_LINK_KEY_TYPE (DEFAULT_GLOBAL_TRUST_CENTER_LINK_KEY)
/* bdbScanDuration */
#define BDB_SCAN_DURATION (0x04)

/* bdbTCLinkKeyExchangeAttempts */
#define BDB_TC_LINK_KEY_EXCHANGE_ATTEMPTS (0x00)
/* bdbTCLinkKeyExchangeAttemptsMax */
#define BDB_TC_LINK_KEY_EXCHANGE_ATTEMPTS_MAX (0x03)
/* bdbTCLinkKeyExchangeMethod */
#define BDB_TC_LINK_KEY_EXCHANGE_METHOD (0x00)
/* bdbTrustCenterNodeJoinTimeout */
#define BDB_TRUST_CENTER_NODE_JOIN_TIMEOUT (0x0F)
/* bdbTrustCenterRequireKeyExchange */
#define BDB_TRUST_CENTER_REQUIRE_KEYEXCHANGE (TRUE)

#endif /* BDB_OPTIONS_INCLUDED */
