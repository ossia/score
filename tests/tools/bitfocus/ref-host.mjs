// Reference host: reproduces companion's ConnectionChildHandlerLegacy (1.x modules)
// closely enough to serve as the oracle for score's host.
import { spawn } from 'node:child_process'
import fs from 'node:fs'
import path from 'node:path'
import dgram from 'node:dgram'
import EJSON from 'ejson'
import semver from 'semver'
import osc from 'osc'

const args = Object.fromEntries(
	process.argv.slice(2).reduce((acc, a, i, arr) => (a.startsWith('--') ? [...acc, [a.slice(2), arr[i + 1]]] : acc), [])
)
const moduleDir = path.resolve(args.module)
const outFile = args.out
const settleMs = Number(args.settle ?? 2500)
const gapMs = Number(args.gap ?? 120)
const maxActions = Number(args['max-actions'] ?? 400)
const NODE_RUNTIME = args['node-runtime']
const out = fs.createWriteStream(outFile)
const emit = (o) => out.write(JSON.stringify({ t: (performance.timeOrigin + performance.now()) / 1000, ...o }) + '\n')

const manifest = JSON.parse(fs.readFileSync(path.join(moduleDir, 'companion/manifest.json'), 'utf8'))
const apiVersion = manifest.runtime.apiVersion
const nodeMajor = manifest.runtime.type === 'node18' ? '18.20.5' : '22.12.0'
const nodePath = path.join(NODE_RUNTIME, nodeMajor, 'bin/node')
const entrypoint = path.join(moduleDir, 'companion', manifest.runtime.entrypoint)
const usesSeparateUpgrade = semver.satisfies(semver.coerce(apiVersion), '>=1.13.0')
const expectsLabelUpdates = semver.satisfies(semver.coerce(apiVersion), '>=1.2.0')
const LABEL = args.label ?? 'dev'
const HOST_OVERRIDE = args.host ?? '10.99.0.2'
const scenario = args.scenario ? JSON.parse(fs.readFileSync(args.scenario, 'utf8')) : null

const sleep = (ms) => new Promise((r) => setTimeout(r, ms))

class Ipc {
	constructor(child, handlers) {
		this.child = child
		this.handlers = handlers
		this.next = 1
		this.pending = new Map()
		child.on('message', (m) => this.received(m))
	}
	call(name, msg, timeout = 5000) {
		const id = this.next++
		return new Promise((resolve, reject) => {
			const to = setTimeout(() => {
				this.pending.delete(id)
				reject(new Error('Call timed out: ' + name))
			}, timeout)
			this.pending.set(id, { resolve, reject, to })
			this.child.send({ direction: 'call', name, payload: EJSON.stringify(msg), callbackId: id })
		})
	}
	notify(name, msg) {
		this.child.send({ direction: 'call', name, payload: EJSON.stringify(msg), callbackId: undefined })
	}
	received(msg) {
		if (msg.direction === 'call') {
			const h = this.handlers[msg.name]
			const data = msg.payload ? EJSON.parse(msg.payload) : undefined
			if (!h) {
				emit({ ev: 'unhandled-call', name: msg.name })
				if (msg.callbackId)
					this.child.send({
						direction: 'response',
						callbackId: msg.callbackId,
						success: false,
						payload: EJSON.stringify({ message: `Unknown command "${msg.name}"` }),
					})
				return
			}
			Promise.resolve()
				.then(() => h(data))
				.then(
					(res) =>
						msg.callbackId &&
						this.child.send({ direction: 'response', callbackId: msg.callbackId, success: true, payload: EJSON.stringify(res) }),
					(err) =>
						msg.callbackId &&
						this.child.send({
							direction: 'response',
							callbackId: msg.callbackId,
							success: false,
							payload: JSON.stringify(err, Object.getOwnPropertyNames(err)),
						})
				)
		} else if (msg.direction === 'response') {
			const p = this.pending.get(msg.callbackId)
			if (!p) return
			this.pending.delete(msg.callbackId)
			clearTimeout(p.to)
			const data = msg.payload ? EJSON.parse(msg.payload) : undefined
			if (msg.success) p.resolve(data)
			else p.reject(data && typeof data === 'object' && 'message' in data ? new Error(data.message) : data)
		}
	}
}

function defaultOptions(def) {
	const o = {}
	for (const opt of def.options ?? []) {
		if (opt.type === 'static-text') continue
		if ('default' in opt) o[opt.id] = structuredClone(opt.default)
	}
	return o
}

function isHostField(f) {
	return (
		(f.type === 'textinput' || f.type === 'bonjour-device') &&
		/(^|_|-)(host|ip|ipaddress|address|addr|server|device_?ip|target)($|_|-)|^(host|ip)/i.test(f.id)
	)
}

async function startModule(phase, initMsg) {
	const state = {
		phase,
		actions: {},
		feedbacks: {},
		variables: {},
		variableValues: {},
		presets: 0,
		savedConfig: undefined,
		savedSecrets: undefined,
		status: [],
		udp: new Map(),
	}
	const child = spawn(nodePath, ['--enable-source-maps', entrypoint], {
		cwd: moduleDir,
		env: {
			...process.env,
			CONNECTION_ID: 'conn-' + phase,
			VERIFICATION_TOKEN: 'tok',
			MODULE_MANIFEST: 'companion/manifest.json',
		},
		stdio: ['pipe', 'pipe', 'pipe', 'ipc'],
		serialization: 'json',
	})
	const lines = (stream, kind) => {
		let buf = ''
		stream.on('data', (d) => {
			buf += d
			let i
			while ((i = buf.indexOf('\n')) >= 0) {
				emit({ ev: kind, phase, line: buf.slice(0, i).slice(0, 2000) })
				buf = buf.slice(i + 1)
			}
		})
	}
	lines(child.stdout, 'stdout')
	lines(child.stderr, 'stderr')
	state.exited = new Promise((r) =>
		child.on('exit', (code, signal) => {
			emit({ ev: 'exit', phase, code, signal })
			r({ code, signal })
		})
	)
	let registered
	const registeredP = new Promise((r) => (registered = r))
	const oscPort = new osc.UDPPort({ localAddress: '0.0.0.0', localPort: 0, metadata: true })
	oscPort.open()

	const handlers = {
		register: async () => registered(),
		'log-message': async (m) => emit({ ev: 'log', phase, level: m.level, message: String(m.message).slice(0, 2000) }),
		'set-status': async (m) => {
			state.status.push(m)
			emit({ ev: 'status', phase, status: m.status, message: m.message })
		},
		setActionDefinitions: async (m) => {
			state.actions = Object.fromEntries((m.actions || []).map((a) => [a.id, a]))
			emit({ ev: 'defs', phase, kind: 'actions', count: m.actions.length })
		},
		setFeedbackDefinitions: async (m) => {
			state.feedbacks = Object.fromEntries((m.feedbacks || []).map((a) => [a.id, a]))
			emit({ ev: 'defs', phase, kind: 'feedbacks', count: m.feedbacks.length })
		},
		setVariableDefinitions: async (m) => {
			state.variables = Object.fromEntries(m.variables.map((v) => [v.id, v]))
			for (const v of m.newValues ?? []) state.variableValues[v.id] = v.value
			emit({ ev: 'defs', phase, kind: 'variables', count: m.variables.length })
		},
		setPresetDefinitions: async (m) => {
			state.presets = m.presets.length
			emit({ ev: 'defs', phase, kind: 'presets', count: m.presets.length })
		},
		setVariableValues: async (m) => {
			for (const v of m.newValues) state.variableValues[v.id] = v.value
			emit({ ev: 'vars', phase, values: m.newValues.slice(0, 50) })
		},
		updateFeedbackValues: async (m) => {
			state.lastFeedbackValues ??= {}
			for (const v of m.values) state.lastFeedbackValues[v.id] = v.value
			emit({ ev: 'fbvals', phase, values: m.values })
		},
		saveConfig: async (m) => {
			if (m.config) state.savedConfig = m.config
			if (m.secrets) state.savedSecrets = m.secrets
			emit({ ev: 'saveConfig', phase, config: m.config, secrets: m.secrets })
		},
		'send-osc': async (m) => {
			emit({ ev: 'send-osc', phase, host: m.host, port: m.port, path: m.path, args: m.args })
			try {
				oscPort.send({ address: m.path, args: m.args }, m.host, m.port)
			} catch (e) {
				emit({ ev: 'send-osc-error', phase, message: String(e) })
			}
		},
		parseVariablesInString: async (m) => {
			const ids = []
			const text = m.text.replace(/\$\(([^:$)]+):([^)$]+)\)/g, (all, lbl, v) => {
				ids.push(`${lbl}:${v}`)
				if (lbl === LABEL && v in state.variableValues) return String(state.variableValues[v])
				return '$NA'
			})
			return { text, variableIds: ids }
		},
		upgradedItems: async () => undefined,
		recordAction: async () => undefined,
		setCustomVariable: async (m) => emit({ ev: 'setCustomVariable', phase, m }),
		sharedUdpSocketJoin: async (m) => {
			const sock = dgram.createSocket({ type: m.family, reuseAddr: true })
			const handleId = 'h' + Math.random().toString(16).slice(2)
			await new Promise((res, rej) => {
				sock.once('error', rej)
				sock.bind(m.portNumber, () => res())
			})
			sock.on('message', (message, rinfo) =>
				ipc.notify('sharedUdpSocketMessage', { handleId, portNumber: m.portNumber, message, source: rinfo })
			)
			sock.on('error', (error) => ipc.notify('sharedUdpSocketError', { handleId, portNumber: m.portNumber, error }))
			state.udp.set(handleId, sock)
			emit({ ev: 'udp-join', phase, port: m.portNumber })
			return handleId
		},
		sharedUdpSocketLeave: async (m) => {
			state.udp.get(m.handleId)?.close()
			state.udp.delete(m.handleId)
		},
		sharedUdpSocketSend: async (m) => {
			emit({ ev: 'udp-send', phase, address: m.address, port: m.port, bytes: Buffer.from(m.message).toString('hex') })
			state.udp.get(m.handleId)?.send(m.message, m.port, m.address)
		},
	}
	const ipc = new Ipc(child, handlers)
	state.ipc = ipc
	state.child = child
	state.close = () => {
		for (const s of state.udp.values()) s.close()
		oscPort.close()
	}

	const ok = await Promise.race([registeredP.then(() => true), sleep(15000).then(() => false), state.exited.then(() => false)])
	if (!ok) {
		emit({ ev: 'register-failed', phase })
		return state
	}
	await sleep(20)
	emit({ ev: 'registered', phase })
	try {
		const res = await ipc.call('init', initMsg(state), 10000)
		state.initResponse = res
		emit({
			ev: 'init-ok',
			phase,
			hasHttpHandler: res.hasHttpHandler,
			newUpgradeIndex: res.newUpgradeIndex,
			updatedConfig: res.updatedConfig,
		})
	} catch (e) {
		emit({ ev: 'init-failed', phase, message: String(e?.message ?? e) })
		state.initFailed = true
	}
	try {
		state.fields = (await ipc.call('getConfigFields', {})).fields
		emit({ ev: 'config-fields', phase, fields: state.fields })
	} catch (e) {
		emit({ ev: 'config-fields-failed', phase, message: String(e?.message ?? e) })
	}
	return state
}

async function stop(state) {
	if (!state.child || state.child.exitCode !== null) return
	try {
		await state.ipc.call('destroy', {}, 3000)
	} catch (e) {
		emit({ ev: 'destroy-failed', phase: state.phase, message: String(e?.message ?? e) })
	}
	state.child.kill('SIGKILL')
	await Promise.race([state.exited, sleep(2000)])
	state.close?.()
}

// Phase 1: a brand-new connection, as when a user adds it in companion.
const probe = await startModule('probe', () => ({
	label: LABEL,
	isFirstInit: true,
	config: {},
	secrets: {},
	lastUpgradeIndex: -1,
	actions: {},
	feedbacks: {},
}))
await sleep(300)
// companion keeps what init returns, which supersedes what was saved during init
const baseConfig = structuredClone(probe.initResponse?.updatedConfig ?? probe.savedConfig ?? {})
const baseSecrets = structuredClone(probe.savedSecrets ?? probe.initResponse?.updatedSecrets ?? {})
const upgradeIndex = probe.initResponse?.newUpgradeIndex ?? -1
const overrides = {}
for (const f of probe.fields ?? []) {
	if (isHostField(f) && !baseConfig[f.id]) baseConfig[f.id] = overrides[f.id] = HOST_OVERRIDE
}
for (const [k, v] of Object.entries(scenario?.config ?? {})) baseConfig[k] = overrides[k] = v

await stop(probe)
emit({ ev: 'config', config: baseConfig, secrets: baseSecrets, upgradeIndex })
fs.writeFileSync(outFile + '.config.json', JSON.stringify({ config: baseConfig, secrets: baseSecrets, overrides, upgradeIndex, fields: probe.fields ?? [] }))

// Phase 2: companion restarting with that saved connection.
const makeActionInstances = (state) =>
	scenario?.actions
		? scenario.actions.map((a, i) => ({
				id: 'act' + i,
				controlId: 'bank:1:' + i,
				actionId: a.id,
				options: { ...defaultOptions(state.actions[a.id] ?? {}), ...a.options },
				upgradeIndex: upgradeIndex,
				disabled: false,
			}))
		: Object.values(state.actions)
		.sort((a, b) => (a.id < b.id ? -1 : a.id > b.id ? 1 : 0))
		.slice(0, maxActions)
		.map((def, i) => ({
			id: 'act' + i,
			controlId: 'bank:1:' + i,
			actionId: def.id,
			options: defaultOptions(def),
			upgradeIndex: upgradeIndex,
			disabled: false,
		}))
const makeFeedbackInstances = (state) =>
	Object.values(state.feedbacks).map((def, i) => ({
		id: 'fb' + i,
		controlId: 'bank:2:' + i,
		feedbackId: def.id,
		options: defaultOptions(def),
		isInverted: false,
		image: { width: 72, height: 58 },
		upgradeIndex: upgradeIndex,
		disabled: false,
	}))

const run = await startModule('run', () => ({
	label: LABEL,
	isFirstInit: false,
	config: baseConfig,
	secrets: baseSecrets,
	lastUpgradeIndex: upgradeIndex,
	actions: {},
	feedbacks: {},
}))
if (run.initResponse) {
	const fbs = makeFeedbackInstances(run)
	try {
		await run.ipc.call('updateFeedbacks', { feedbacks: Object.fromEntries(fbs.map((f) => [f.id, f])) })
	} catch (e) {
		emit({ ev: 'updateFeedbacks-failed', message: String(e?.message ?? e) })
	}
	const acts = makeActionInstances(run)
	try {
		await run.ipc.call('updateActions', { actions: Object.fromEntries(acts.map((a) => [a.id, { ...a }])) })
	} catch (e) {
		emit({ ev: 'updateActions-failed', message: String(e?.message ?? e) })
	}
	await sleep(settleMs)
	emit({ ev: 'actions-begin' })
	const pending = []
	for (const a of acts) {
		emit({ ev: 'action', id: a.actionId, options: a.options })
		pending.push(
			run.ipc.call('executeAction', { action: a, surfaceId: undefined }, 5000).then(
				(r) => emit({ ev: 'action-result', id: a.actionId, success: r ? r.success : true, error: r?.errorMessage }),
				(e) => emit({ ev: 'action-result', id: a.actionId, success: false, error: String(e?.message ?? e) })
			)
		)
		await sleep(gapMs)
	}
	await Promise.all(pending)
	for (const inj of scenario?.inject ?? []) {
		const sock = dgram.createSocket('udp4')
		await new Promise((r) => sock.send(Buffer.from(inj.hex, 'hex'), inj.port, '127.0.0.1', r))
		sock.close()
		emit({ ev: 'inject', port: inj.port, hex: inj.hex })
		await sleep(inj.wait ?? 300)
	}
	emit({ ev: 'actions-end' })
	await sleep(500)
}
const feedbackValues = {}
{
	const byInstance = {}
	for (const f of makeFeedbackInstances(run)) byInstance[f.id] = f.feedbackId
	for (const [id, v] of Object.entries(run.lastFeedbackValues ?? {})) feedbackValues[byInstance[id] ?? id] = v
}
emit({
	ev: 'summary',
	feedbackValues,
	actions: Object.keys(run.actions).length,
	feedbacks: Object.keys(run.feedbacks).length,
	variables: Object.keys(run.variables).length,
	variableValues: run.variableValues,
	presets: run.presets,
})
await stop(run)
out.end()
process.exit(0)
