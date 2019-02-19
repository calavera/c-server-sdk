#include "ldinternal.h"
#include "ldevaluate.h"

static struct LDJSON *
variation(struct LDClient *const client, const struct LDUser *const user,
    const char *const key, struct LDJSON *const fallback,
    const LDJSONType type, struct LDJSON **const details)
{
    bool status;
    struct LDJSON *flag;
    struct LDJSON *value;
    struct LDStore *store;

    if (!client) {
        if (!addErrorReason(details, "NULL_CLIENT")) {
            LD_LOG(LD_LOG_ERROR, "failed to add error reason");

            goto error;
        }

        goto fallback;
    }

    if (!key) {
        if (!addErrorReason(details, "NULL_KEY")) {
            LD_LOG(LD_LOG_ERROR, "failed to add error reason");

            return NULL;
        }

        goto fallback;
    }

    if (!user) {
        if (!addErrorReason(details, "USER_NOT_SPECIFIED")) {
            LD_LOG(LD_LOG_ERROR, "failed to add error reason");

            goto error;
        }

        goto fallback;
    }

    LD_ASSERT(store = client->config->store);

    flag = LDStoreGet(store, "flags", key);

    if (!flag) {
        if (!addErrorReason(details, "FLAG_NOT_FOUND")) {
            LD_LOG(LD_LOG_ERROR, "failed to add error reason");

            goto error;
        }

        goto fallback;
    }

    status = evaluate(flag, user, store, details);

    if (!status) {
        goto fallback;
    }

    value = LDObjectLookup(*details, "value");

    LD_ASSERT(details);

    if (LDJSONGetType(value) != type) {
        if (!addErrorReason(details, "WRONG_TYPE")) {
            LD_LOG(LD_LOG_ERROR, "failed to add error reason");

            goto error;
        }

        goto fallback;
    }

    value = LDJSONDuplicate(value);

    LDJSONFree(fallback);

    return value;

  fallback:
    return fallback;

  error:
    *details = NULL;

    LDJSONFree(fallback);

    return NULL;
}

bool
LDBoolVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const bool fallback,
    struct LDJSON **const details)
{
    bool value;
    struct LDJSON *result;
    struct LDJSON *fallbackJSON;

    if (!(fallbackJSON = LDNewBool(fallback))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        return fallback;
    }

    result = variation(client, user, key, fallbackJSON, LDBool, details);

    if (!result) {
        LD_LOG(LD_LOG_ERROR, "LDVariation internal failure");

        return fallback;
    }

    value = LDGetBool(result);

    LDJSONFree(result);

    return value;
}

int
LDIntVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const int fallback,
    struct LDJSON **const details)
{
    int value;
    struct LDJSON *result;
    struct LDJSON *fallbackJSON;

    if (!(fallbackJSON = LDNewNumber(fallback))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        return fallback;
    }

    result = variation(client, user, key, fallbackJSON, LDNumber, details);

    if (!result) {
        LD_LOG(LD_LOG_ERROR, "LDVariation internal failure");

        return fallback;
    }

    value = LDGetNumber(result);

    LDJSONFree(result);

    return value;
}

double
LDDoubleVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const double fallback,
    struct LDJSON **const details)
{
    double value;
    struct LDJSON *result;
    struct LDJSON *fallbackJSON;

    if (!(fallbackJSON = LDNewNumber(fallback))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        return fallback;
    }

    result = variation(client, user, key, fallbackJSON, LDNumber, details);

    if (!result) {
        LD_LOG(LD_LOG_ERROR, "LDVariation internal failure");

        return fallback;
    }

    value = LDGetNumber(result);

    LDJSONFree(result);

    return value;
}

char *
LDStringVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const char* const fallback,
    struct LDJSON **const details)
{
    const char *value;
    struct LDJSON *result;
    struct LDJSON *fallbackJSON = NULL;

    if (fallback && !(fallbackJSON = LDNewText(fallback))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        if (fallback) {
            return strdup(fallback);
        } else {
            return NULL;
        }
    }

    result = variation(client, user, key, fallbackJSON, LDText, details);

    if (!result) {
        LD_LOG(LD_LOG_ERROR, "LDVariation internal failure");

        if (fallback) {
            strdup(fallback);
        } else {
            return NULL;
        }
    }

    value = LDGetText(result);

    LDJSONFree(result);

    if (value) {
        return strdup(value);
    } else {
        return NULL;
    }
}

struct LDJSON *
LDJSONVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const struct LDJSON *const fallback,
    struct LDJSON **const details)
{
    struct LDJSON *result;
    struct LDJSON *fallbackJSON = NULL;

    if (fallback && !(fallbackJSON = LDJSONDuplicate(fallback))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        return NULL;
    }

    result = variation(client, user, key, fallbackJSON, LDText, details);

    if (!result) {
        LD_LOG(LD_LOG_ERROR, "LDVariation internal failure");

        if (fallback) {
            return LDJSONDuplicate(fallback);
        } else {
            return NULL;
        }
    }

    return result;
}
