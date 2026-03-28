src = open('interp.c').read()

# Fix while loop
src = src.replace(
    '''        case NODE_WHILE: {
            while (!interp->error && !g_returning) {
                Value cond = interp_eval(interp, node->as.while_stmt.cond, env);
                if (interp->error || !val_truthy(cond)) break;
                Env *child = env_new(env);
                interp_eval(interp, node->as.while_stmt.body, child);
                env_free(child);
            }
            return val_null();
        }''',
    '''        case NODE_WHILE: {
            while (!interp->error && !g_returning && !g_breaking) {
                Value cond = interp_eval(interp, node->as.while_stmt.cond, env);
                if (interp->error || !val_truthy(cond)) break;
                Env *child = env_new(env);
                interp_eval(interp, node->as.while_stmt.body, child);
                env_free(child);
                if (g_breaking) { g_breaking = 0; break; }
                if (g_continuing) { g_continuing = 0; }
            }
            return val_null();
        }'''
)

# Fix for loop
src = src.replace(
    '''        case NODE_FOR: {
            Env *loop_env = env_new(env);
            interp_eval(interp, node->as.for_stmt.init, loop_env);
            while (!interp->error && !g_returning) {
                Value cond = interp_eval(interp, node->as.for_stmt.cond, loop_env);
                if (interp->error || !val_truthy(cond)) break;
                Env *body_env = env_new(loop_env);
                interp_eval(interp, node->as.for_stmt.body, body_env);
                env_free(body_env);
                if (interp->error || g_returning) break;
                interp_eval(interp, node->as.for_stmt.step, loop_env);
            }
            env_free(loop_env);
            return val_null();
        }''',
    '''        case NODE_FOR: {
            Env *loop_env = env_new(env);
            interp_eval(interp, node->as.for_stmt.init, loop_env);
            while (!interp->error && !g_returning && !g_breaking) {
                Value cond = interp_eval(interp, node->as.for_stmt.cond, loop_env);
                if (interp->error || !val_truthy(cond)) break;
                Env *body_env = env_new(loop_env);
                interp_eval(interp, node->as.for_stmt.body, body_env);
                env_free(body_env);
                if (g_breaking) { g_breaking = 0; break; }
                if (g_continuing) { g_continuing = 0;
                    interp_eval(interp, node->as.for_stmt.step, loop_env);
                    continue; }
                if (interp->error || g_returning) break;
                interp_eval(interp, node->as.for_stmt.step, loop_env);
            }
            env_free(loop_env);
            return val_null();
        }'''
)

open('interp.c','w').write(src)
print("done")
