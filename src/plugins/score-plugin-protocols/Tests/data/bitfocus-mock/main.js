// Stands in for a companion module built on @companion-module/base 1.14: speaks
// the same nodejs-ipc protocol, and records what the host sends in $MOCK_LOG.
const fs = require('fs')

const log = (o) => {
	try {
		if (process.env.MOCK_LOG) fs.appendFileSync(process.env.MOCK_LOG, JSON.stringify(o) + '\n')
	} catch (e) {}
}

// The subset of EJSON the protocol uses
const ejsonParse = (s) =>
	JSON.parse(s, (k, v) => {
		if (v && typeof v === 'object' && Object.keys(v).length === 1 && '$binary' in v)
			return Buffer.from(v.$binary, 'base64')
		return v
	})
const ejsonStringify = (o) =>
	JSON.stringify(o, function (k, v) {
		const raw = this[k]
		if (raw instanceof Uint8Array) return { $binary: Buffer.from(raw).toString('base64') }
		return v
	})

// String ids, as module-base < 1.5 (nanoid) used
let nextId = 1
const pending = new Map()
const call = (name, msg) =>
	new Promise((resolve, reject) => {
		const id = 'cb' + nextId++
		pending.set(id, { resolve, reject })
		process.send({ direction: 'call', name, payload: ejsonStringify(msg), callbackId: id })
	})
const notify = (name, msg) => process.send({ direction: 'call', name, payload: ejsonStringify(msg) })

const actions = [
	{
		id: 'typed',
		name: 'Typed options',
		options: [
			{ type: 'static-text', id: 'info', label: 'Info', value: 'text' },
			// Default is a string while the ids are numbers: companion sends it untouched
			{ type: 'dropdown', id: 'mixed', label: 'Mixed', default: '1', choices: [{ id: 1, label: 'One' }, { id: 2, label: 'Two' }] },
			{ type: 'dropdown', id: 'numeric', label: 'Numeric', default: 2, choices: [{ id: 1, label: 'One' }, { id: 2, label: 'Two' }] },
			{ type: 'multidropdown', id: 'multi', label: 'Multi', default: [], choices: [{ id: 'a', label: 'A' }, { id: 'b', label: 'B' }] },
			{ type: 'number', id: 'int', label: 'Int', default: 3, min: 0, max: 10 },
			{ type: 'number', id: 'float', label: 'Float', default: 0.5, min: 0, max: 1, step: 0.1 },
			{ type: 'checkbox', id: 'check', label: 'Check', default: true },
			{ type: 'colorpicker', id: 'color', label: 'Color', default: 0xff0000 },
			{ type: 'textinput', id: 'text', label: 'Text', default: 'hello' },
			{ type: 'textinput', id: 'nodefault', label: 'No default' },
			{ type: 'number', id: 'tenth', label: 'Tenth', default: 0.1, min: 0, max: 1 },
			{ type: 'number', id: 'numnodefault', label: 'Number without default', min: 0, max: 10 },
			// Defaults of the "wrong" shape, which companion passes through
			{ type: 'number', id: 'numstring', label: 'Number with a text default', default: '', min: 0, max: 10 },
			{ type: 'checkbox', id: 'boolnum', label: 'Bool as number', default: 0 },
			{ type: 'multidropdown', id: 'multiscalar', label: 'Scalar multi', default: 'a', choices: [{ id: 'a', label: 'A' }] },
		],
	},
	{ id: 'single', name: 'One option', options: [{ type: 'dropdown', id: 'choice', label: 'Choice', default: 1, choices: [{ id: 1, label: 'One' }, { id: 2, label: 'Two' }] }] },
	{ id: 'fail', name: 'Fails', options: [] },
	{ id: 'crash', name: 'Crashes the module', options: [] },
	{ id: 'churn', name: 'Sends its definitions again and again', options: [] },
	{ id: 'many', name: 'Defines many variables', options: [{ type: 'number', id: 'count', label: 'Count', default: 10, min: 0, max: 100000 }] },
	{ id: 'undeclared', name: 'Sets an undeclared variable', options: [] },
	{ id: 'levels', name: 'Numbers of both kinds', options: [] },
	{ id: 'redefine', name: 'Adds an action', options: [] },
	{ id: 'udp', name: 'Shared UDP', options: [{ type: 'number', id: 'port', label: 'Port', default: 0, min: 0, max: 65535 }] },
	{ id: 'osc', name: 'Send OSC', options: [{ type: 'number', id: 'port', label: 'Port', default: 0, min: 0, max: 65535 }] },
]

const feedbacks = [
	{
		id: 'state',
		name: 'State',
		type: 'boolean',
		defaultStyle: { bgcolor: 0xff0000, color: 0 },
		options: [{ type: 'dropdown', id: 'which', label: 'Which', default: 'on', choices: [{ id: 'on', label: 'On' }, { id: 'off', label: 'Off' }] }],
	},
	{ id: 'level', name: 'Level', type: 'value', options: [] },
]

const configFields = [
	{ type: 'textinput', id: 'host', label: 'Host', width: 6, default: '' },
	{ type: 'number', id: 'port', label: 'Port', width: 6, default: 1234, min: 1, max: 65535 },
	{ type: 'secret-text', id: 'password', label: 'Password', width: 6 },
	{ type: 'number', id: 'txPort', label: 'Port without default', width: 6, min: 6000, max: 6100 },
	{ type: 'dropdown', id: 'iface', label: 'Default outside of the choices', width: 6, default: 'Pick one', choices: [{ id: 'eth0', label: 'eth0' }] },
	{ type: 'textinput', id: 'poll', label: 'Filled by the module', width: 6 },
	{ type: 'textinput', id: 'late', label: 'Filled by the module later', width: 6 },
	{ type: 'checkbox', id: 'debug', label: 'Checkbox without default', width: 6 },
	{ type: 'checkbox', id: 'weird', label: 'Checkbox with a number default', width: 6, default: 2 },
	{ type: 'dropdown', id: 'mode', label: 'Numeric choice', width: 6, default: 2, choices: [{ id: 1, label: 'One' }, { id: 2, label: 'Two' }] },
]

let config = {}
let udpHandle

const handlers = {
	init: async (msg) => {
		log({ ev: 'init', msg, connectionId: process.env.CONNECTION_ID })
		// A device that never answers
		if (msg.config.hang) await new Promise(() => {})
		config = msg.config
		// As an upgrade script would
		if (config.version === 'old') config = { ...config, version: 'new' }
		if (msg.isFirstInit) {
			config = { host: '', port: 1234, poll: 3 }
			notify('saveConfig', { config, secrets: {} })
		}
		// module-base < 1.13 waits for this reply before finishing init
		await call('upgradedItems', { updatedActions: {}, updatedFeedbacks: {} })
		notify('setActionDefinitions', { actions })
		notify('setFeedbackDefinitions', { feedbacks })
		notify('setVariableDefinitions', {
			variables: [
				{ id: 'name', name: 'Name' },
				{ id: 'count', name: 'Count' },
			],
			newValues: [
				{ id: 'name', value: 'mock' },
				{ id: 'count', value: 42 },
			],
		})
		// A value learnt from the device, after init
		setTimeout(() => {
			config = { ...config, late: 'learnt' }
			notify('saveConfig', { config, secrets: {} })
		}, 500)
		return {
			hasHttpHandler: false,
			hasRecordActionsHandler: false,
			newUpgradeIndex: 3,
			disableNewConfigLayout: false,
			updatedConfig: config,
			updatedSecrets: msg.secrets,
		}
	},
	getConfigFields: async () => ({ fields: configFields }),
	updateConfigAndLabel: async (msg) => log({ ev: 'updateConfigAndLabel', msg }),
	updateFeedbacks: async (msg) => {
		log({ ev: 'updateFeedbacks', msg })
		const values = []
		for (const [id, fb] of Object.entries(msg.feedbacks)) {
			if (!fb) continue
			if (fb.feedbackId === 'state') values.push({ id, controlId: fb.controlId, value: fb.options.which === 'on' })
			if (fb.feedbackId === 'level') values.push({ id, controlId: fb.controlId, value: 0.25 })
		}
		notify('updateFeedbackValues', { values })
	},
	updateActions: async () => undefined,
	executeAction: async (msg) => {
		log({ ev: 'executeAction', msg })
		const a = msg.action
		switch (a.actionId) {
			case 'fail':
				return { success: false, errorMessage: 'expected failure' }
			case 'churn': {
				const end = Date.now() + 1500
				const again = () => {
					notify('setActionDefinitions', { actions })
					if (Date.now() < end) setTimeout(again, 1)
				}
				again()
				break
			}
			case 'many': {
				const vars = [{ id: 'name', name: 'Name' }, { id: 'count', name: 'Count' }]
				for (let i = 0; i < a.options.count; i++) vars.push({ id: 'v' + i, name: 'Variable ' + i })
				notify('setVariableDefinitions', { variables: vars, newValues: [] })
				notify('setVariableValues', { newValues: [{ id: 'name', value: 'many:' + a.options.count }] })
				break
			}
			case 'levels': {
				const seq = [0.5, 1, 0.75, 2, 0.25]
				seq.forEach((v, i) => setTimeout(() => notify('setVariableValues', { newValues: [{ id: 'count', value: v }] }), 50 * i))
				setTimeout(() => notify('setVariableValues', { newValues: [{ id: 'name', value: 1790000000123 }] }), 300)
				break
			}
			case 'undeclared':
				notify('setVariableValues', { newValues: [{ id: 'extra', value: 'hello' }] })
				break
			case 'crash':
				setImmediate(() => {
					throw new Error('crash')
				})
				break
			case 'redefine':
				notify('setActionDefinitions', { actions: [...actions, { id: 'added', name: 'Added', options: [] }] })
				notify('setVariableValues', { newValues: [{ id: 'count', value: 43 }] })
				break
			case 'udp':
				udpHandle = await call('sharedUdpSocketJoin', { family: 'udp4', portNumber: a.options.port })
				log({ ev: 'udpJoined', handle: udpHandle })
				break
			case 'osc':
				notify('send-osc', {
					host: '127.0.0.1',
					port: a.options.port,
					path: '/mock',
					args: [{ type: 'i', value: 7 }, { type: 's', value: 'x' }, { type: 'b', value: Buffer.from([1, 2, 3]) }],
				})
				break
		}
		return { success: true, errorMessage: undefined }
	},
	sharedUdpSocketMessage: async (msg) => {
		log({
			ev: 'udpMessage',
			isBuffer: Buffer.isBuffer(msg.message),
			data: Buffer.from(msg.message).toString('hex'),
			source: msg.source,
			sameHandle: msg.handleId === udpHandle,
		})
		// Echo back through the host's socket
		await call('sharedUdpSocketSend', {
			handleId: udpHandle,
			message: Buffer.concat([Buffer.from('echo:'), msg.message]),
			address: msg.source.address,
			port: msg.source.port,
		})
	},
	destroy: async () => log({ ev: 'destroy' }),
}

process.on('message', (msg) => {
	if (msg.direction === 'response') {
		const p = pending.get(msg.callbackId)
		if (!p) return
		pending.delete(msg.callbackId)
		const data = msg.payload ? ejsonParse(msg.payload) : undefined
		if (msg.success) p.resolve(data)
		else p.reject(data)
		return
	}
	const h = handlers[msg.name]
	const data = msg.payload ? ejsonParse(msg.payload) : undefined
	Promise.resolve(h ? h(data) : undefined).then(
		(res) =>
			msg.callbackId &&
			process.send({ direction: 'response', callbackId: msg.callbackId, success: true, payload: ejsonStringify(res) }),
		(err) =>
			msg.callbackId &&
			process.send({ direction: 'response', callbackId: msg.callbackId, success: false, payload: JSON.stringify({ message: String(err) }) })
	)
})

call('register', { apiVersion: '1.14.1', connectionId: process.env.CONNECTION_ID, verificationToken: process.env.VERIFICATION_TOKEN }).catch(
	(e) => {
		log({ ev: 'registerFailed', e: String(e) })
		process.exit(11)
	}
)
