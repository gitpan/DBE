#ifndef __DBE_SQL_H__
#define __DBE_SQL_H__ 1

/* begin: SQL datatype codes (do NOT remove this line!) */
enum sql_type {
	SQL_TINYINT							= (-6),
	SQL_BIGINT							= (-5),
	SQL_LONGVARBINARY					= (-4),
	SQL_VARBINARY						= (-3),
	SQL_BINARY							= (-2),
	SQL_LONGVARCHAR						= (-1),
	SQL_UNKNOWN_TYPE					= 0,
	SQL_ALL_TYPES						= 0,
	SQL_CHAR							= 1,
	SQL_NUMERIC							= 2,
	SQL_DECIMAL							= 3,
	SQL_INTEGER							= 4,
	SQL_SMALLINT						= 5,
	SQL_FLOAT							= 6,
	SQL_REAL							= 7,
	SQL_DOUBLE							= 8,
	SQL_DATETIME						= 9,
	SQL_DATE							= 9,
	SQL_TIME							= 10,
	SQL_TIMESTAMP						= 11,
	SQL_VARCHAR							= 12,
	SQL_BOOLEAN							= 16,
	SQL_BLOB							= 30,
	SQL_BLOB_LOCATOR					= 31,
	SQL_CLOB							= 40,
	SQL_CLOB_LOCATOR					= 41,
	SQL_TYPE_DATE						= 91,
	SQL_TYPE_TIME						= 92,
	SQL_TYPE_TIMESTAMP					= 93,
	SQL_TYPE_TIME_WITH_TIMEZONE			= 94,
	SQL_TYPE_TIMESTAMP_WITH_TIMEZONE	= 95,
	SQL_INTERVAL_YEAR					= 101,
	SQL_INTERVAL_MONTH					= 102,
	SQL_INTERVAL_DAY					= 103,
	SQL_INTERVAL_HOUR					= 104,
	SQL_INTERVAL_MINUTE					= 105,
	SQL_INTERVAL_SECOND					= 106,
	SQL_INTERVAL_YEAR_TO_MONTH			= 107,
	SQL_INTERVAL_DAY_TO_HOUR			= 108,
	SQL_INTERVAL_DAY_TO_MINUTE			= 109,
	SQL_INTERVAL_DAY_TO_SECOND			= 110,
	SQL_INTERVAL_HOUR_TO_MINUTE			= 111,
	SQL_INTERVAL_HOUR_TO_SECOND			= 112,
	SQL_INTERVAL_MINUTE_TO_SECOND		= 113,
};
/* end: SQL datatype codes (do NOT remove this line!) */

/* begin: getinfo codes (do NOT remove this line!) */
enum sql_info_type {
	SQL_DATA_SOURCE_NAME				=     2,
	SQL_DRIVER_NAME						=     6,
	SQL_DRIVER_VER						=     7,
	SQL_SERVER_NAME						=    13,
	SQL_SEARCH_PATTERN_ESCAPE			=    14,
	SQL_DATABASE_NAME					=    16,
	SQL_DBMS_NAME						=    17,
	SQL_DBMS_VER						=    18,
	SQL_ACCESSIBLE_TABLES				=    19,
	SQL_ACCESSIBLE_PROCEDURES			=    20,
	SQL_PROCEDURES						=    21,
	SQL_CONCAT_NULL_BEHAVIOR			=    22,
	SQL_CURSOR_COMMIT_BEHAVIOR			=    23,
	SQL_CURSOR_ROLLBACK_BEHAVIOR		=    24,
	SQL_EXPRESSIONS_IN_ORDERBY			=    27,
	SQL_IDENTIFIER_CASE					=    28,
	SQL_IDENTIFIER_QUOTE_CHAR			=    29,
	SQL_MAX_COLUMN_NAME_LEN				=    30,
	SQL_MAX_CURSOR_NAME_LEN				=    31,
	SQL_MAX_SCHEMA_NAME_LEN				=    32,
	SQL_MAX_CATALOG_NAME_LEN			=    34,
	SQL_MAX_TABLE_NAME_LEN				=    35,
	SQL_MULT_RESULT_SETS				=    36,
	SQL_SCHEMA_TERM						=    39,
	SQL_PROCEDURE_TERM					=    40,
	SQL_CATALOG_NAME_SEPARATOR			=    41,
	SQL_CATALOG_TERM					=    42,
	SQL_TABLE_TERM						=    45,
	SQL_TXN_CAPABLE						=    46,
	SQL_USER_NAME						=    47,
	SQL_CONVERT_FUNCTIONS				=    48,
	SQL_NUMERIC_FUNCTIONS				=    49,
	SQL_STRING_FUNCTIONS				=    50,
	SQL_SYSTEM_FUNCTIONS				=    51,
	SQL_TIMEDATE_FUNCTIONS				=    52,
	SQL_TXN_ISOLATION_OPTION			=    72,
	SQL_CORRELATION_NAME				=    74,
	SQL_NON_NULLABLE_COLUMNS			=    75,
	SQL_NULL_COLLATION					=    85,
	SQL_ALTER_TABLE						=    86,
	SQL_COLUMN_ALIAS					=    87,
	SQL_GROUP_BY						=    88,
	SQL_KEYWORDS						=    89,
	SQL_ORDER_BY_COLUMNS_IN_SELECT		=    90,
	SQL_SCHEMA_USAGE					=    91,
	SQL_CATALOG_USAGE					=    92,
	SQL_QUOTED_IDENTIFIER_CASE			=    93,
	SQL_SPECIAL_CHARACTERS				=    94,
	SQL_SUBQUERIES						=    95,
	SQL_UNION							=    96,
	SQL_MAX_COLUMNS_IN_GROUP_BY			=    97,
	SQL_MAX_COLUMNS_IN_INDEX			=    98,
	SQL_MAX_COLUMNS_IN_ORDER_BY			=    99,
	SQL_MAX_COLUMNS_IN_SELECT			=   100,
	SQL_MAX_COLUMNS_IN_TABLE			=   101,
	SQL_MAX_INDEX_SIZE					=   102,
	SQL_MAX_ROW_SIZE_INCLUDES_LONG		=   103,
	SQL_MAX_ROW_SIZE					=   104,
	SQL_MAX_STATEMENT_LEN				=   105,
	SQL_MAX_TABLES_IN_SELECT			=   106,
	SQL_MAX_USER_NAME_LEN				=   107,
	SQL_MAX_CHAR_LITERAL_LEN			=   108,
	SQL_NEED_LONG_DATA_LEN				=   111,
	SQL_LIKE_ESCAPE_CLAUSE				=   113,
	SQL_CATALOG_LOCATION				=   114,
	SQL_OJ_CAPABILITIES					=   115,
	SQL_ALTER_DOMAIN					=   117,
	SQL_SQL_CONFORMANCE					=   118,
	SQL_CREATE_SCHEMA					=   131,
	SQL_CREATE_TABLE					=   132,
	SQL_CREATE_VIEW						=   134,
	SQL_DROP_SCHEMA						=   140,
	SQL_DROP_TABLE						=   141,
	SQL_DROP_VIEW						=   143,
	SQL_INDEX_KEYWORDS					=   148,
	SQL_SQL92_DATETIME_FUNCTIONS		=   155,
	SQL_SQL92_FOREIGN_KEY_UPDATE_RULE	=   157,
	SQL_SQL92_GRANT						=   158,
	SQL_SQL92_PREDICATES				=   160,
	SQL_SQL92_RELATIONAL_JOIN_OPERATORS	=   161,
	SQL_SQL92_ROW_VALUE_CONSTRUCTOR		=   163,
	SQL_SQL92_STRING_FUNCTIONS			=   164,
	SQL_AGGREGATE_FUNCTIONS				=   169,
	SQL_DDL_INDEX						=   170,
	SQL_INSERT_STATEMENT				=   172,
	SQL_DESCRIBE_PARAMETER				= 10002,
	SQL_CATALOG_NAME					= 10003,
	SQL_MAX_IDENTIFIER_LEN				= 10005,
};
/* end: getinfo codes (do NOT remove this line!) */

/* SQL_AGGREGATE_FUNCTIONS bitmasks */
#define SQL_AF_AVG								0x00000001L
#define SQL_AF_COUNT							0x00000002L
#define SQL_AF_MAX								0x00000004L
#define SQL_AF_MIN								0x00000008L
#define SQL_AF_SUM								0x00000010L
#define SQL_AF_DISTINCT							0x00000020L
#define SQL_AF_ALL								0x00000040L

/* SQL_CATALOG_LOCATION values */
#define SQL_CL_START							1
#define SQL_CL_END								2

/* SQL_CATALOG_USAGE masks */
#define SQL_CU_DML_STATEMENTS					0x00000001L
#define SQL_CU_PROCEDURE_INVOCATION				0x00000002L
#define SQL_CU_TABLE_DEFINITION					0x00000004L
#define SQL_CU_INDEX_DEFINITION					0x00000008L
#define SQL_CU_PRIVILEGE_DEFINITION				0x00000010L

/* SQL_CONCAT_NULL_BEHAVIOR values */
#define SQL_CB_NULL								0x0000
#define SQL_CB_NON_NULL							0x0001

/* SQL_CONVERT_FUNCTIONS functions */
#define SQL_FN_CVT_CONVERT						0x00000001L
#define SQL_FN_CVT_CAST							0x00000002L

/* SQL_CORRELATION_NAME values */
#define SQL_CN_NONE								0x0000
#define SQL_CN_DIFFERENT						0x0001
#define SQL_CN_ANY								0x0002

/* SQL_CREATE_SCHEMA bitmasks */
#define	SQL_CS_CREATE_SCHEMA					0x00000001L
#define	SQL_CS_AUTHORIZATION					0x00000002L
#define	SQL_CS_DEFAULT_CHARACTER_SET			0x00000004L

/* SQL_CREATE_TABLE bitmasks */
#define	SQL_CT_CREATE_TABLE						0x00000001L
#define	SQL_CT_COMMIT_PRESERVE					0x00000002L
#define	SQL_CT_COMMIT_DELETE					0x00000004L
#define	SQL_CT_GLOBAL_TEMPORARY					0x00000008L
#define	SQL_CT_LOCAL_TEMPORARY					0x00000010L
#define	SQL_CT_CONSTRAINT_INITIALLY_DEFERRED	0x00000020L
#define	SQL_CT_CONSTRAINT_INITIALLY_IMMEDIATE	0x00000040L
#define	SQL_CT_CONSTRAINT_DEFERRABLE			0x00000080L
#define	SQL_CT_CONSTRAINT_NON_DEFERRABLE		0x00000100L
#define SQL_CT_COLUMN_CONSTRAINT				0x00000200L
#define SQL_CT_COLUMN_DEFAULT					0x00000400L
#define SQL_CT_COLUMN_COLLATION					0x00000800L
#define SQL_CT_TABLE_CONSTRAINT					0x00001000L
#define SQL_CT_CONSTRAINT_NAME_DEFINITION		0x00002000L

/* SQL_CREATE_VIEW values */
#define	SQL_CV_CREATE_VIEW						0x00000001L
#define	SQL_CV_CHECK_OPTION						0x00000002L
#define	SQL_CV_CASCADED							0x00000004L
#define	SQL_CV_LOCAL							0x00000008L

/* SQL_CURSOR_COMMIT_BEHAVIOR values */
#define SQL_CB_DELETE							0
#define SQL_CB_CLOSE							1
#define SQL_CB_PRESERVE							2

/* SQL_DDL_INDEX bitmasks */
#define SQL_DI_CREATE_INDEX						0x00000001L
#define SQL_DI_DROP_INDEX						0x00000002L

/* SQL_DROP_SCHEMA bitmasks */
#define	SQL_DS_DROP_SCHEMA						0x00000001L
#define SQL_DS_RESTRICT							0x00000002L
#define	SQL_DS_CASCADE							0x00000004L

/* SQL_DROP_TABLE bitmasks */
#define	SQL_DT_DROP_TABLE						0x00000001L
#define	SQL_DT_RESTRICT							0x00000002L
#define	SQL_DT_CASCADE							0x00000004L

/* SQL_DROP_VIEW bitmasks */
#define	SQL_DV_DROP_VIEW						0x00000001L
#define	SQL_DV_RESTRICT							0x00000002L
#define	SQL_DV_CASCADE							0x00000004L

/* SQL_GROUP_BY values */
#define SQL_GB_NOT_SUPPORTED					0x0000
#define SQL_GB_GROUP_BY_EQUALS_SELECT			0x0001
#define SQL_GB_GROUP_BY_CONTAINS_SELECT 	    0x0002
#define SQL_GB_NO_RELATION          	        0x0003
#define	SQL_GB_COLLATE							0x0004

/* SQL_IDENTIFIER_CASE values */
#define SQL_IC_UPPER							1
#define SQL_IC_LOWER							2
#define SQL_IC_SENSITIVE						3
#define SQL_IC_MIXED							4

/* Bitmasks for SQL_INDEX_KEYWORDS */
#define SQL_IK_NONE								0x00000000L
#define SQL_IK_ASC								0x00000001L
#define SQL_IK_DESC								0x00000002L
#define SQL_IK_ALL								0x00000003L

/* SQL_INSERT_STATEMENT bitmasks */
#define	SQL_IS_INSERT_LITERALS					0x00000001L
#define SQL_IS_INSERT_SEARCHED					0x00000002L
#define SQL_IS_SELECT_INTO						0x00000004L

/* SQL_NON_NULLABLE_COLUMNS values */
#define SQL_NNC_NULL							0x0000
#define SQL_NNC_NON_NULL						0x0001

/* SQL_NULL_COLLATION values */
#define SQL_NC_HIGH								0x0000
#define SQL_NC_LOW								0x0001
#define SQL_NC_START							0x0002
#define SQL_NC_END								0x0004

/* SQL_NUMERIC_FUNCTIONS functions */
#define SQL_FN_NUM_ABS							0x00000001L
#define SQL_FN_NUM_ACOS							0x00000002L
#define SQL_FN_NUM_ASIN							0x00000004L
#define SQL_FN_NUM_ATAN							0x00000008L
#define SQL_FN_NUM_ATAN2						0x00000010L
#define SQL_FN_NUM_CEILING						0x00000020L
#define SQL_FN_NUM_COS							0x00000040L
#define SQL_FN_NUM_COT							0x00000080L
#define SQL_FN_NUM_EXP							0x00000100L
#define SQL_FN_NUM_FLOOR						0x00000200L
#define SQL_FN_NUM_LOG							0x00000400L
#define SQL_FN_NUM_MOD							0x00000800L
#define SQL_FN_NUM_SIGN							0x00001000L
#define SQL_FN_NUM_SIN							0x00002000L
#define SQL_FN_NUM_SQRT							0x00004000L
#define SQL_FN_NUM_TAN							0x00008000L
#define SQL_FN_NUM_PI							0x00010000L
#define SQL_FN_NUM_RAND							0x00020000L
#define SQL_FN_NUM_DEGREES						0x00040000L
#define SQL_FN_NUM_LOG10						0x00080000L
#define SQL_FN_NUM_POWER						0x00100000L
#define SQL_FN_NUM_RADIANS						0x00200000L
#define SQL_FN_NUM_ROUND						0x00400000L
#define SQL_FN_NUM_TRUNCATE						0x00800000L

/* SQL_OJ_CAPABILITIES bitmasks */
#define SQL_OJ_LEFT								0x00000001L
#define SQL_OJ_RIGHT							0x00000002L
#define SQL_OJ_FULL								0x00000004L
#define SQL_OJ_NESTED							0x00000008L
#define SQL_OJ_NOT_ORDERED						0x00000010L
#define SQL_OJ_INNER							0x00000020L
#define SQL_OJ_ALL_COMPARISON_OPS				0x00000040L

/* SQL_SCHEMA_USAGE masks */
#define SQL_SU_DML_STATEMENTS					0x00000001L
#define SQL_SU_PROCEDURE_INVOCATION				0x00000002L
#define SQL_SU_TABLE_DEFINITION					0x00000004L
#define SQL_SU_INDEX_DEFINITION					0x00000008L
#define SQL_SU_PRIVILEGE_DEFINITION				0x00000010L

/* SQL_SQL_CONFORMANCE bit masks */
#define	SQL_SC_SQL92_ENTRY						0x00000001L
#define	SQL_SC_FIPS127_2_TRANSITIONAL			0x00000002L
#define	SQL_SC_SQL92_INTERMEDIATE				0x00000004L
#define	SQL_SC_SQL92_FULL						0x00000008L

/* SQL_SQL92_DATETIME_FUNCTIONS */
#define SQL_SDF_CURRENT_DATE					0x00000001L
#define SQL_SDF_CURRENT_TIME					0x00000002L
#define SQL_SDF_CURRENT_TIMESTAMP				0x00000004L

/* SQL_SQL92_FOREIGN_KEY_UPDATE_RULE bitmasks */
#define SQL_SFKU_CASCADE						0x00000001L
#define SQL_SFKU_NO_ACTION						0x00000002L
#define SQL_SFKU_SET_DEFAULT					0x00000004L
#define SQL_SFKU_SET_NULL						0x00000008L

/* SQL_SQL92_GRANT	bitmasks */
#define SQL_SG_USAGE_ON_DOMAIN					0x00000001L
#define SQL_SG_USAGE_ON_CHARACTER_SET			0x00000002L
#define SQL_SG_USAGE_ON_COLLATION				0x00000004L
#define SQL_SG_USAGE_ON_TRANSLATION				0x00000008L
#define SQL_SG_WITH_GRANT_OPTION				0x00000010L
#define SQL_SG_DELETE_TABLE						0x00000020L
#define SQL_SG_INSERT_TABLE						0x00000040L
#define SQL_SG_INSERT_COLUMN					0x00000080L
#define SQL_SG_REFERENCES_TABLE					0x00000100L
#define SQL_SG_REFERENCES_COLUMN				0x00000200L
#define SQL_SG_SELECT_TABLE						0x00000400L
#define SQL_SG_UPDATE_TABLE						0x00000800L
#define SQL_SG_UPDATE_COLUMN					0x00001000L	

/* SQL_SQL92_PREDICATES bitmasks */
#define SQL_SP_EXISTS							0x00000001L
#define SQL_SP_ISNOTNULL						0x00000002L
#define SQL_SP_ISNULL							0x00000004L
#define SQL_SP_MATCH_FULL						0x00000008L
#define SQL_SP_MATCH_PARTIAL					0x00000010L
#define SQL_SP_MATCH_UNIQUE_FULL				0x00000020L
#define SQL_SP_MATCH_UNIQUE_PARTIAL				0x00000040L
#define SQL_SP_OVERLAPS							0x00000080L
#define SQL_SP_UNIQUE							0x00000100L
#define SQL_SP_LIKE								0x00000200L
#define SQL_SP_IN								0x00000400L
#define SQL_SP_BETWEEN							0x00000800L
#define SQL_SP_COMPARISON						0x00001000L
#define SQL_SP_QUANTIFIED_COMPARISON			0x00002000L

/* SQL_SQL92_RELATIONAL_JOIN_OPERATORS bitmasks */
#define SQL_SRJO_CORRESPONDING_CLAUSE			0x00000001L
#define SQL_SRJO_CROSS_JOIN						0x00000002L
#define SQL_SRJO_EXCEPT_JOIN					0x00000004L
#define SQL_SRJO_FULL_OUTER_JOIN				0x00000008L
#define SQL_SRJO_INNER_JOIN						0x00000010L
#define SQL_SRJO_INTERSECT_JOIN					0x00000020L
#define SQL_SRJO_LEFT_OUTER_JOIN				0x00000040L
#define SQL_SRJO_NATURAL_JOIN					0x00000080L
#define SQL_SRJO_RIGHT_OUTER_JOIN				0x00000100L
#define SQL_SRJO_UNION_JOIN						0x00000200L

/* SQL_SQL92_ROW_VALUE_CONSTRUCTOR bitmasks */
#define SQL_SRVC_VALUE_EXPRESSION				0x00000001L
#define SQL_SRVC_NULL							0x00000002L
#define SQL_SRVC_DEFAULT						0x00000004L
#define SQL_SRVC_ROW_SUBQUERY					0x00000008L

/* SQL_SQL92_STRING_FUNCTIONS */
#define SQL_SSF_CONVERT							0x00000001L	
#define SQL_SSF_LOWER							0x00000002L
#define SQL_SSF_UPPER							0x00000004L
#define SQL_SSF_SUBSTRING						0x00000008L
#define SQL_SSF_TRANSLATE						0x00000010L
#define SQL_SSF_TRIM_BOTH						0x00000020L
#define SQL_SSF_TRIM_LEADING					0x00000040L
#define SQL_SSF_TRIM_TRAILING					0x00000080L

/* SQL_STRING_FUNCTIONS functions */
#define SQL_FN_STR_CONCAT						0x00000001L
#define SQL_FN_STR_INSERT						0x00000002L
#define SQL_FN_STR_LEFT							0x00000004L
#define SQL_FN_STR_LTRIM						0x00000008L
#define SQL_FN_STR_LENGTH						0x00000010L
#define SQL_FN_STR_LOCATE						0x00000020L
#define SQL_FN_STR_LCASE						0x00000040L
#define SQL_FN_STR_REPEAT						0x00000080L
#define SQL_FN_STR_REPLACE						0x00000100L
#define SQL_FN_STR_RIGHT						0x00000200L
#define SQL_FN_STR_RTRIM						0x00000400L
#define SQL_FN_STR_SUBSTRING					0x00000800L
#define SQL_FN_STR_UCASE						0x00001000L
#define SQL_FN_STR_ASCII						0x00002000L
#define SQL_FN_STR_CHAR							0x00004000L
#define SQL_FN_STR_DIFFERENCE					0x00008000L
#define SQL_FN_STR_LOCATE_2						0x00010000L
#define SQL_FN_STR_SOUNDEX						0x00020000L
#define SQL_FN_STR_SPACE						0x00040000L
#define SQL_FN_STR_BIT_LENGTH					0x00080000L
#define SQL_FN_STR_CHAR_LENGTH					0x00100000L
#define SQL_FN_STR_CHARACTER_LENGTH				0x00200000L
#define SQL_FN_STR_OCTET_LENGTH					0x00400000L
#define SQL_FN_STR_POSITION						0x00800000L

/* SQL_SUBQUERIES masks */
#define SQL_SQ_COMPARISON						0x00000001L
#define SQL_SQ_EXISTS							0x00000002L
#define SQL_SQ_IN								0x00000004L
#define SQL_SQ_QUANTIFIED						0x00000008L
#define SQL_SQ_CORRELATED_SUBQUERIES			0x00000010L

/* SQL_SYSTEM_FUNCTIONS functions */
#define SQL_FN_SYS_USERNAME						0x00000001L
#define SQL_FN_SYS_DBNAME						0x00000002L
#define SQL_FN_SYS_IFNULL						0x00000004L

/* SQL_TIMEDATE_FUNCTIONS functions */
#define SQL_FN_TD_NOW							0x00000001L
#define SQL_FN_TD_CURDATE						0x00000002L
#define SQL_FN_TD_DAYOFMONTH					0x00000004L
#define SQL_FN_TD_DAYOFWEEK						0x00000008L
#define SQL_FN_TD_DAYOFYEAR						0x00000010L
#define SQL_FN_TD_MONTH							0x00000020L
#define SQL_FN_TD_QUARTER						0x00000040L
#define SQL_FN_TD_WEEK							0x00000080L
#define SQL_FN_TD_YEAR							0x00000100L
#define SQL_FN_TD_CURTIME						0x00000200L
#define SQL_FN_TD_HOUR							0x00000400L
#define SQL_FN_TD_MINUTE						0x00000800L
#define SQL_FN_TD_SECOND						0x00001000L
#define SQL_FN_TD_TIMESTAMPADD					0x00002000L
#define SQL_FN_TD_TIMESTAMPDIFF					0x00004000L
#define SQL_FN_TD_DAYNAME						0x00008000L
#define SQL_FN_TD_MONTHNAME						0x00010000L
#define SQL_FN_TD_CURRENT_DATE					0x00020000L
#define SQL_FN_TD_CURRENT_TIME					0x00040000L
#define SQL_FN_TD_CURRENT_TIMESTAMP				0x00080000L
#define SQL_FN_TD_EXTRACT						0x00100000L

/* SQL_TXN_CAPABLE values */
#define SQL_TC_NONE								0
#define SQL_TC_DML								1
#define SQL_TC_ALL								2
#define SQL_TC_DDL_COMMIT						3
#define SQL_TC_DDL_IGNORE						4

/* SQL_TXN_ISOLATION_OPTION bitmasks */
#define SQL_TXN_READ_UNCOMMITTED				0x00000001L
#define SQL_TXN_READ_COMMITTED					0x00000002L
#define SQL_TXN_REPEATABLE_READ					0x00000004L
#define SQL_TXN_SERIALIZABLE					0x00000008L

/* SQL_UNION masks */
#define SQL_U_UNION								0x00000001L
#define SQL_U_UNION_ALL							0x00000002L

/* SQLColAttributes subdefines for SQL_COLUMN_SEARCHABLE */
/* These are also used by SQLGetInfo                     */
#define SQL_UNSEARCHABLE						0
#define SQL_LIKE_ONLY							1
#define SQL_ALL_EXCEPT_LIKE						2
#define SQL_SEARCHABLE							3

/* Column types and scopes in SQLSpecialColumns.  */
#define SQL_BEST_ROWID							1
#define SQL_ROWVER								2

/* Values that may appear in the result set of SQLSpecialColumns() */
#define SQL_SCOPE_CURROW						0
#define SQL_SCOPE_TRANSACTION					1
#define SQL_SCOPE_SESSION						2

/* Defines for SQLSpecialColumns (returned in the result set) */
#define SQL_PC_UNKNOWN							0
#define SQL_PC_NON_PSEUDO						1
#define SQL_PC_PSEUDO							2

#endif	/* __DBE_SQL_H__ */
