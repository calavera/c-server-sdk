#include "ldinternal.h"
#include "ldnetwork.h"

#include <stdio.h>
#include <unistd.h>

#include <curl/curl.h>

const char *const agentheader = "User-Agent: CServerClient/0.1";

bool
prepareShared(const struct LDConfig *const config, const char *const url,
    CURL **const o_curl, struct curl_slist **const o_headers)
{
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;

    LD_ASSERT(config);
    LD_ASSERT(url);
    LD_ASSERT(o_curl);
    LD_ASSERT(o_headers);

    if (!(curl = curl_easy_init())) {
        LD_LOG(LD_LOG_CRITICAL, "curl_easy_init returned NULL");

        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK) {
        LD_LOG(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_URL failed on");

        goto error;
    }

    {
        char headerauth[256];

        if (snprintf(headerauth, sizeof(headerauth), "Authorization: %s",
            config->key) < 0)
        {
            LD_LOG(LD_LOG_CRITICAL, "snprintf during failed");

            goto error;
        }

        LD_LOG(LD_LOG_INFO, "using authentication: %s", headerauth);

        if (!(headers = curl_slist_append(headers, headerauth))) {
            LD_LOG(LD_LOG_CRITICAL, "curl_slist_append failed for headerauth");

            goto error;
        }
    }

    if (!(headers = curl_slist_append(headers, agentheader))) {
        LD_LOG(LD_LOG_CRITICAL, "curl_slist_append failed for headeragent");

        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers) != CURLE_OK) {
        LD_LOG(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_HTTPHEADER failed");

        goto error;
    }

    *o_curl = curl;
    *o_headers = headers;

    return true;

  error:
    curl_easy_cleanup(curl);

    curl_slist_free_all(headers);

    return false;
}

THREAD_RETURN
LDi_networkthread(void* const clientref)
{
    struct LDClient *const client = clientref;

    struct NetworkInterface *interfaces[3];

    const size_t interfacecount =
        sizeof(interfaces) / sizeof(struct NetworkInterface *);

    CURLM *multihandle;

    LD_ASSERT(client);

    if (!(multihandle = curl_multi_init())) {
        LD_LOG(LD_LOG_ERROR, "failed to construct multihandle");

        return THREAD_RETURN_DEFAULT;
    }

    if (!(interfaces[0] = constructPolling(client))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct polling");

        return THREAD_RETURN_DEFAULT;
    }

    if (!(interfaces[1] = constructStreaming(client))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct streaming");

        return THREAD_RETURN_DEFAULT;
    }

    if (!(interfaces[2] = constructAnalytics(client))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct analytics");

        return THREAD_RETURN_DEFAULT;
    }

    while (true) {
        struct CURLMsg *info = NULL;
        int running_handles = 0;
        int active_events = 0;

        LD_ASSERT(LDi_rdlock(&client->lock));
        if (client->shuttingdown) {
            LD_ASSERT(LDi_rdunlock(&client->lock));

            break;
        }
        LD_ASSERT(LDi_rdunlock(&client->lock));

        curl_multi_perform(multihandle, &running_handles);

        for (unsigned int i = 0; i < interfacecount; i++) {
            CURL *const handle =
                interfaces[i]->poll(client, interfaces[i]->context);

            if (handle) {
                interfaces[i]->current = handle;

                if (curl_easy_setopt(
                    handle, CURLOPT_PRIVATE, interfaces[i]) != CURLE_OK)
                {
                    LD_LOG(LD_LOG_ERROR, "failed to associate context");

                    goto cleanup;
                }

                if (curl_multi_add_handle(
                    multihandle, handle) != CURLM_OK)
                {
                    LD_LOG(LD_LOG_ERROR, "failed to add handle");

                    goto cleanup;
                }
            }
        }

        do {
            int inqueue = 0;

            info = curl_multi_info_read(multihandle, &inqueue);

            if (info && (info->msg == CURLMSG_DONE)) {
                long responsecode;
                CURL *easy = info->easy_handle;
                struct NetworkInterface *interface = NULL;

                if (curl_easy_getinfo(
                    easy, CURLINFO_RESPONSE_CODE, &responsecode) != CURLE_OK)
                {
                    LD_LOG(LD_LOG_ERROR, "failed to get response code");

                    goto cleanup;
                }

                LD_LOG(LD_LOG_INFO, "message done code %d %d",
                    info->data.result, responsecode);

                if (curl_easy_getinfo(
                    easy, CURLINFO_PRIVATE, &interface) != CURLE_OK)
                {
                    LD_LOG(LD_LOG_ERROR, "failed to get context");

                    goto cleanup;
                }

                LD_ASSERT(interface);
                LD_ASSERT(interface->done);
                LD_ASSERT(interface->context);

                interface->done(client, interface->context);

                interface->current = NULL;

                LD_ASSERT(curl_multi_remove_handle(
                    multihandle, easy) == CURLM_OK);

                curl_easy_cleanup(easy);
            }
        } while (info);

        if (curl_multi_wait(
            multihandle, NULL, 0, 5, &active_events) != CURLM_OK)
        {
            LD_LOG(LD_LOG_ERROR, "failed to wait on handles");

            goto cleanup;
        }

        if (!active_events) {
            /* if curl is not doing anything wait so we don't burn CPU */
            usleep(1000 * 10);
        }
    }

  cleanup:
    for (unsigned int i = 0; i < interfacecount; i++) {
        struct NetworkInterface *const interface = interfaces[i];

        if (interface->current) {
            LD_ASSERT(curl_multi_remove_handle(
                multihandle, interface->current) == CURLM_OK);

            curl_easy_cleanup(interface->current);
        }

        interface->destroy(interface->context);
        free(interface);
    }

    LD_ASSERT(curl_multi_cleanup(multihandle) == CURLM_OK);

    return THREAD_RETURN_DEFAULT;
}
