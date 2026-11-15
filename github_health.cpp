// Entry point for WLED v16+ usermod builds (enable with `custom_usermods = github_health`).
#include "usermod_github_health.h"

static GithubHealthUsermod githubHealthUsermod;
REGISTER_USERMOD(githubHealthUsermod);
