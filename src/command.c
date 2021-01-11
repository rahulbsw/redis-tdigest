/*
 * src/command.c
 *
 * Copyright (c) 2016, Usman Masood <usmanm at fastmail dot fm>
 */

#include <limits.h>
#include <string.h>
#include "command.h"
#include "tdigest.h"


#define TYPE_NAME "t-digest0"
#define ENCODING_VER 0

static RedisModuleType *TDigestType;
/* ============== Helper methods =====================================*/
static int RMUtil_ArgIndex(const char *arg, RedisModuleString **argv, int argc) {

  size_t larg = strlen(arg);
  for (int offset = 0; offset < argc; offset++) {
    size_t l;
    const char *carg = RedisModule_StringPtrLen(argv[offset], &l);
    if (l != larg) continue;
    if (carg != NULL && strncasecmp(carg, arg, larg) == 0) {
      return offset;
    }
  }
  return -1;
}

/* ========================== "tdigest" type commands ======================= */

/* TDIGEST.NEW key compression */
static int TDigestTypeNew_RedisCommand(RedisModuleCtx *ctx,
        RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc != 2 && argc != 3)
        return RedisModule_WrongArity(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
            REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (type != REDISMODULE_KEYTYPE_EMPTY) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    long long compression;

    if (argc == 3)
    {
        if ((RedisModule_StringToLongLong(argv[2], &compression) != REDISMODULE_OK)
                || compression <= 0 || compression > INT_MAX) {
            return RedisModule_ReplyWithError(ctx,
                    "ERR invalid compression: must be a positive 32 bit integer");
        }
    }
    else
        compression = DEFAULT_COMPRESSION;

    struct TDigest *t = tdigestNew(compression);
    RedisModule_ModuleTypeSetValue(key, TDigestType, t);

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    RedisModule_ReplicateVerbatim(ctx);

    return REDISMODULE_OK;
}

/* TDIGEST.REPLACE key value count [value count ...] [compression] */
static int TDigestTypeReplace_RedisCommand(RedisModuleCtx *ctx,
        RedisModuleString **argv, int argc) {
  RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc < 4 || argc % 2 != 0)
        return RedisModule_WrongArity(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
            REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (type != REDISMODULE_KEYTYPE_EMPTY
            && RedisModule_ModuleTypeGetType(key) != TDigestType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }
 
    int num_added = (argc - 2) / 2;
    double *values = RedisModule_PoolAlloc(ctx, sizeof(double) * num_added);
    long long *counts = RedisModule_PoolAlloc(ctx, sizeof(long long) * num_added);

    /* Validate all values and weights before trying to add them to ensure atomicity */
    int i;
    for (i = 1; i <= num_added; i++)
    {
        int idx = i * 2;

        double value;
        if (RedisModule_StringToDouble(argv[idx], &value) != REDISMODULE_OK) {
            return RedisModule_ReplyWithError(ctx,
                    "ERR invalid value: must be a double");
        }

        long long count;
        if ((RedisModule_StringToLongLong(argv[idx + 1], &count) != REDISMODULE_OK)
                || count <= 0) {
            return RedisModule_ReplyWithError(ctx,
                    "ERR invalid count: must be a positive 64 bit integer");
        }

        values[i - 1] = value;
        counts[i - 1] = count;
    }

    struct TDigest *t;
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        int    compression = DEFAULT_COMPRESSION;
        t = tdigestNew(DEFAULT_COMPRESSION);
        RedisModule_ModuleTypeSetValue(key, TDigestType, t);
    } else {
        struct TDigest *t1 = RedisModule_ModuleTypeGetValue(key);
        t = tdigestNew(t1->compression);
        RedisModule_ModuleTypeSetValue(key, TDigestType, t);
    }

    long long total_count = 0;
    for (i = 0; i < num_added; i++)
    {
        tdigestAdd(t, values[i], counts[i]);
        total_count += counts[i];
    }

    RedisModule_ReplyWithLongLong(ctx, total_count);
    RedisModule_ReplicateVerbatim(ctx);

    return REDISMODULE_OK;
}

/* TDIGEST.ADD key value count [value count ...] */
static int TDigestTypeAdd_RedisCommand(RedisModuleCtx *ctx,
        RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc < 4 || argc % 2 != 0)
        return RedisModule_WrongArity(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
            REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (type != REDISMODULE_KEYTYPE_EMPTY
            && RedisModule_ModuleTypeGetType(key) != TDigestType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }
 
    int num_added = (argc - 2) / 2;
    double *values = RedisModule_PoolAlloc(ctx, sizeof(double) * num_added);
    long long *counts = RedisModule_PoolAlloc(ctx, sizeof(long long) * num_added);

    /* Validate all values and weights before trying to add them to ensure atomicity */
    int i;
    for (i = 1; i <= num_added; i++)
    {
        int idx = i * 2;

        double value;
        if (RedisModule_StringToDouble(argv[idx], &value) != REDISMODULE_OK) {
            return RedisModule_ReplyWithError(ctx,
                    "ERR invalid value: must be a double");
        }

        long long count;
        if ((RedisModule_StringToLongLong(argv[idx + 1], &count) != REDISMODULE_OK)
                || count <= 0) {
            return RedisModule_ReplyWithError(ctx,
                    "ERR invalid count: must be a positive 64 bit integer");
        }

        values[i - 1] = value;
        counts[i - 1] = count;
    }

    struct TDigest *t;
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        t = tdigestNew(DEFAULT_COMPRESSION);
        RedisModule_ModuleTypeSetValue(key, TDigestType, t);
    } else {
        t = RedisModule_ModuleTypeGetValue(key);
    }

    long long total_count = 0;
    for (i = 0; i < num_added; i++)
    {
        tdigestAdd(t, values[i], counts[i]);
        total_count += counts[i];
    }

    RedisModule_ReplyWithLongLong(ctx, total_count);
    RedisModule_ReplicateVerbatim(ctx);

    return REDISMODULE_OK;
}

/* TDIGEST.MERGE destkey sourcekey [sourcekey ...] */
static int TDigestTypeMerge_RedisCommand(RedisModuleCtx *ctx,
        RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx); /* Use automatic memory management. */
    int num_keys = 0;
    int i, j;

    if (RedisModule_IsKeysPositionRequest(ctx)) {
        for (i = 1; i < argc; i++) {
            RedisModule_KeyAtPos(ctx, i);
        }
        return REDISMODULE_OK;
    }

    if (argc < 3)
        return RedisModule_WrongArity(ctx);

    /* Validate all keys are either empty or tdigest. */
    RedisModuleKey **keys = RedisModule_PoolAlloc(ctx, argc - 1);
    for (i = 1; i < argc; i++) {
        RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[i],
                REDISMODULE_READ | REDISMODULE_WRITE);
        int type = RedisModule_KeyType(key);
        if (type != REDISMODULE_KEYTYPE_EMPTY
                && RedisModule_ModuleTypeGetType(key) != TDigestType) {
            return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        }
        if (i == 1 || type != REDISMODULE_KEYTYPE_EMPTY) {
            keys[num_keys++] = key;
        }
    }

    struct TDigest *t1, *t2;
    if (RedisModule_KeyType(keys[0]) == REDISMODULE_KEYTYPE_EMPTY) {
        t1 = tdigestNew(DEFAULT_COMPRESSION);
        RedisModule_ModuleTypeSetValue(keys[0], TDigestType, t1);
    } else {
        t1 = RedisModule_ModuleTypeGetValue(keys[0]);
    }

    /* Add all centroids from sources to destination. */
    for (i = 1; i < num_keys; i++) {
        t2 = RedisModule_ModuleTypeGetValue(keys[i]);
        for (j = 0; j < t2->num_centroids; j++) {
            tdigestAdd(t1, t2->centroids[j].mean, t2->centroids[j].weight);
        }
    }

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    RedisModule_ReplicateVerbatim(ctx);

    return REDISMODULE_OK;
}

/* TDIGEST.MCDF value [value ...] KEYS sourcekey [sourcekey ...] */
static int TDigestTypeMergeCDF_RedisCommand(RedisModuleCtx *ctx,
        RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx); /* Use automatic memory management. */
    int num_keys = 0;
    int keyword_index=0;
    int i=0, j=0;
    int num_in_value=0;
    int num_in_keys=0;
    
    if (argc < 3)
        return RedisModule_WrongArity(ctx);

    keyword_index = RMUtil_ArgIndex("KEYS",argv, argc);

    if (keyword_index < 2|| keyword_index>argc-1)
        return RedisModule_WrongArity(ctx);

    num_in_keys = argc - (keyword_index+1);

    if(RedisModule_IsKeysPositionRequest(ctx)) {
        for (i = 0; i < num_in_keys; i++) {
            RedisModule_KeyAtPos(ctx, i);
        }   
        return REDISMODULE_OK;
    }
    
    num_in_value=keyword_index-1;
    
    double *values = RedisModule_PoolAlloc(ctx, sizeof(double) * num_in_value);
 
    for (i = 1; i <= num_in_value ; i++)
    {
        double value;
        if (RedisModule_StringToDouble(argv[i], &value) != REDISMODULE_OK) {
            //RedisModule_Log(ctx, "warning", "invalid value: must be a double %s", argv[i]);
            return RedisModule_ReplyWithError(ctx,
                    "ERR invalid value: must be a double");
        }

        values[i-1] = value;
    }
    /* Validate all keys are either empty or tdigest. */
    RedisModuleKey **keys = RedisModule_PoolAlloc(ctx, num_in_keys);
    for (i = 1 ; i <= num_in_keys; i++) {
        RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[i+keyword_index],REDISMODULE_READ | REDISMODULE_WRITE);
        int type = RedisModule_KeyType(key);
        if (type != REDISMODULE_KEYTYPE_EMPTY
                && RedisModule_ModuleTypeGetType(key) != TDigestType) {
            return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        }
        if (type != REDISMODULE_KEYTYPE_EMPTY) {
            keys[num_keys++] = key;
        }
    }
    
    struct TDigest *t,*t2;
    t = tdigestNew(DEFAULT_COMPRESSION);
   
    /* Add all centroids from sources to destination. */
    for (i = 0; i < num_keys; i++) {
        t2 = RedisModule_ModuleTypeGetValue(keys[i]);
        for (j = 0; j < t2->num_centroids; j++) {
            tdigestAdd(t, t2->centroids[j].mean, t2->centroids[j].weight);
        }
    }
    tdigestCompress(t);

    RedisModule_ReplyWithArray(ctx, num_in_value);
    for (i = 0; i < num_in_value; i++)
        RedisModule_ReplyWithDouble(ctx, tdigestCDF(t, values[i]));

    return REDISMODULE_OK;
}


/* TDIGEST.MQUANTILE value [value ...] KEYS sourcekey [sourcekey ...] */
static int TDigestTypeMergeQuantile_RedisCommand(RedisModuleCtx *ctx,
        RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx); /* Use automatic memory management. */
    int num_keys = 0;
    int keyword_index=0;
    int i=0, j=0;
    int num_in_value=0;
    int num_in_keys=0;
    
    if (argc < 3)
        return RedisModule_WrongArity(ctx);

    keyword_index = RMUtil_ArgIndex("KEYS",argv, argc);

    if (keyword_index < 2|| keyword_index>argc-1)
        return RedisModule_WrongArity(ctx);

    num_in_keys = argc - (keyword_index+1);

    if(RedisModule_IsKeysPositionRequest(ctx)) {
        for (i = 0; i < num_in_keys; i++) {
            RedisModule_KeyAtPos(ctx, i);
        }   
        return REDISMODULE_OK;
    }
    
    num_in_value=keyword_index-1;
    
    double *values = RedisModule_PoolAlloc(ctx, sizeof(double) * num_in_value);
 
    for (i = 1; i <= num_in_value ; i++)
    {
        double value;
        if (RedisModule_StringToDouble(argv[i], &value) != REDISMODULE_OK) {
            //RedisModule_Log(ctx, "warning", "invalid value: must be a double %s", argv[i]);
            return RedisModule_ReplyWithError(ctx,
                    "ERR invalid value: must be a double");
        }

        values[i-1] = value;
    }
    /* Validate all keys are either empty or tdigest. */
    RedisModuleKey **keys = RedisModule_PoolAlloc(ctx, num_in_keys);
    for (i = 1 ; i <= num_in_keys; i++) {
        RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[i+keyword_index],REDISMODULE_READ | REDISMODULE_WRITE);
        int type = RedisModule_KeyType(key);
        if (type != REDISMODULE_KEYTYPE_EMPTY
                && RedisModule_ModuleTypeGetType(key) != TDigestType) {
            return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        }
        if (type != REDISMODULE_KEYTYPE_EMPTY) {
            keys[num_keys++] = key;
        }
    }
    
    struct TDigest *t,*t2;
    t = tdigestNew(DEFAULT_COMPRESSION);
   
    /* Add all centroids from sources to destination. */
    for (i = 0; i < num_keys; i++) {
        t2 = RedisModule_ModuleTypeGetValue(keys[i]);
        for (j = 0; j < t2->num_centroids; j++) {
            tdigestAdd(t, t2->centroids[j].mean, t2->centroids[j].weight);
        }
    }
    tdigestCompress(t);

    RedisModule_ReplyWithArray(ctx, num_in_value);
    for (i = 0; i < num_in_value; i++)
        RedisModule_ReplyWithDouble(ctx, tdigestQuantile(t, values[i]));

    return REDISMODULE_OK;
}

/* TDIGEST.CDF key value [value ...] */
static int TDigestTypeCDF_RedisCommand(RedisModuleCtx *ctx,
        RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc < 3)
        return RedisModule_WrongArity(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
            REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }
    else if (RedisModule_ModuleTypeGetType(key) != TDigestType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    double *values = RedisModule_PoolAlloc(ctx, sizeof(double) * argc - 2);

    int i;
    for (i = 2; i < argc; i++)
    {
        double value;
        if (RedisModule_StringToDouble(argv[i], &value) != REDISMODULE_OK) {
            return RedisModule_ReplyWithError(ctx,
                    "ERR invalid value: must be a double");
        }

        values[i - 2] = value;
    }

    struct TDigest *t = RedisModule_ModuleTypeGetValue(key);

    RedisModule_ReplyWithArray(ctx, argc - 2);
    for (i = 0; i < argc - 2; i++)
        RedisModule_ReplyWithDouble(ctx, tdigestCDF(t, values[i]));

    return REDISMODULE_OK;
}

/* TDIGEST.QUANTILE key quantile [quantile ...] */
static int TDigestTypeQuantile_RedisCommand(RedisModuleCtx *ctx,
        RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc < 3)
        return RedisModule_WrongArity(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
            REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }
    else if (RedisModule_ModuleTypeGetType(key) != TDigestType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    double *quantiles = RedisModule_PoolAlloc(ctx, sizeof(double) * argc - 2);

    int i;
    for (i = 2; i < argc; i++)
    {
        double quantile;
        if ((RedisModule_StringToDouble(argv[i], &quantile) != REDISMODULE_OK)
                || quantile < 0 || quantile > 1) {
            return RedisModule_ReplyWithError(ctx,
                    "ERR invalid quantile: must be a double between 0..1");
        }

        quantiles[i - 2] = quantile;
    }

    struct TDigest *t = RedisModule_ModuleTypeGetValue(key);

    RedisModule_ReplyWithArray(ctx, argc - 2);
    for (i = 0; i < argc - 2; i++)
        RedisModule_ReplyWithDouble(ctx, tdigestQuantile(t, quantiles[i]));

    return REDISMODULE_OK;
}

/* TDIGEST.DEBUG key */
static int TDigestTypeDebug_RedisCommand(RedisModuleCtx *ctx,
        RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc != 2)
        return RedisModule_WrongArity(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
            REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }
    else if (RedisModule_ModuleTypeGetType(key) != TDigestType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    struct TDigest *t = RedisModule_ModuleTypeGetValue(key);

    tdigestCompress(t);

    RedisModule_ReplyWithArray(ctx, t->num_centroids + 1);

    char buf[1024];
    sprintf(buf, "TDIGEST (%d, %d, %ld)", (int) t->compression,
            t->num_centroids,
            sizeof(struct TDigest) + sizeof(struct Centroid) * t->num_centroids);
    RedisModule_ReplyWithSimpleString(ctx, buf);

    int i;
    for (i = 0; i < t->num_centroids; i++) {
        struct Centroid *c = &t->centroids[i];
        sprintf(buf, "  CENTROID (%f, %lld)", c->mean, c->weight);
        RedisModule_ReplyWithSimpleString(ctx, buf);
    }

    return REDISMODULE_OK;
}

/* ========================== "tdigest" type methods ======================= */

static void *TDigestTypeRdbLoad(RedisModuleIO *rdb, int encver) {
    if (encver != ENCODING_VER) {
        return NULL;
    }

    uint64_t compression = RedisModule_LoadUnsigned(rdb);
    uint64_t num_centroids = RedisModule_LoadUnsigned(rdb);
    struct TDigest *t = tdigestNew(compression);

    while (num_centroids--) {
        double mean = RedisModule_LoadDouble(rdb);
        long long weight = RedisModule_LoadUnsigned(rdb);

        tdigestAdd(t, mean, weight);
    }
    return t;
}

static void TDigestTypeRdbSave(RedisModuleIO *rdb, void *value) {
    struct TDigest *t = value;

    tdigestCompress(t);

    RedisModule_SaveUnsigned(rdb, t->compression);
    RedisModule_SaveUnsigned(rdb, t->num_centroids);

    int i;
    for (i = 0; i < t->num_centroids; i++) {
        struct Centroid *c = &t->centroids[i];
        RedisModule_SaveDouble(rdb, c->mean);
        RedisModule_SaveUnsigned(rdb, c->weight);
    }
}

static void TDigestTypeAofRewrite(RedisModuleIO *aof, RedisModuleString *key,
        void *value) {
    struct TDigest *t = value;
    int i;

    RedisModule_EmitAOF(aof, "TDIGEST.NEW", "%s %ll", key, t->compression);

    tdigestCompress(t);

    for (i = 0; i < t->num_centroids; i++) {
        struct Centroid *c = &t->centroids[i];
        RedisModule_EmitAOF(aof, "TDIGEST.ADD", "%s %f %ll", key, c->mean,
                c->weight);
    }
}

static size_t TDigestMemUsage(const void *value) {
    const struct TDigest *t = value;
    size_t sz = sizeof(struct TDigest);
    sz += t->num_buffered_pts * sizeof(struct Point);
    sz += t->num_centroids * sizeof(struct Centroid);
    return sz;
}

static void TDigestTypeFree(void *value) {
    tdigestFree(value);
}

int RedisModule_OnLoad(RedisModuleCtx *ctx) {
    if (RedisModule_Init(ctx, TYPE_NAME, 1,
            REDISMODULE_APIVER_1) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    RedisModuleTypeMethods tm = { .version = REDISMODULE_TYPE_METHOD_VERSION,
	                          .rdb_load = TDigestTypeRdbLoad,
				  .rdb_save = TDigestTypeRdbSave,
				  .aof_rewrite = TDigestTypeAofRewrite,
				  .mem_usage = TDigestMemUsage,
				  .free = TDigestTypeFree };

    TDigestType = RedisModule_CreateDataType(ctx, TYPE_NAME, ENCODING_VER, &tm);
    if (TDigestType == NULL)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "tdigest.new",
            TDigestTypeNew_RedisCommand, "write deny-oom", 1, 1,
            1) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "tdigest.add",
            TDigestTypeAdd_RedisCommand, "write deny-oom", 1, 1,
            1) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

     if (RedisModule_CreateCommand(ctx, "tdigest.replace",
            TDigestTypeReplace_RedisCommand, "write deny-oom", 1, 1,
            1) == REDISMODULE_ERR)
        return REDISMODULE_ERR;    

    if (RedisModule_CreateCommand(ctx, "tdigest.merge",
            TDigestTypeMerge_RedisCommand, "write deny-oom getkeys-api", 0, 0,
            0) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "tdigest.cdf",
            TDigestTypeCDF_RedisCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "tdigest.quantile",
            TDigestTypeQuantile_RedisCommand, "readonly", 1, 1,
            1) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "tdigest.debug",
            TDigestTypeDebug_RedisCommand, "readonly", 1, 1,
            1) == REDISMODULE_ERR)
        return REDISMODULE_ERR;
    if (RedisModule_CreateCommand(ctx, "tdigest.mcdf",
            TDigestTypeMergeCDF_RedisCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR)
        return REDISMODULE_ERR;
    if (RedisModule_CreateCommand(ctx, "tdigest.mquantile",
            TDigestTypeMergeQuantile_RedisCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR)
        return REDISMODULE_ERR;
    return REDISMODULE_OK;
}
